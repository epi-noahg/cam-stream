"use client";

/**
 * Onglet Calibration — piloté par le serveur de fléchettes autoritatif.
 *
 * - Affiche en continu la CALIBRATION ACTUELLE dessinée sur le flux live de
 *   chaque caméra (toggle "Calibration actuelle").
 * - Relance un auto-scan (AutoCalibrator) en arrière-plan : l'aperçu live
 *   continue de tourner pendant le scan, et le résultat arrive en broadcast.
 * - Permet d'ajuster les réglages puis d'enregistrer le bon résultat — écrit
 *   sur disque ET appliqué à chaud au pipeline en cours.
 */

import { useEffect, useRef, useState } from "react";
import Link from "next/link";
import { ArrowLeft, Crosshair, ScanLine, Save, RotateCcw, Play, Pause, CheckCircle2, Loader2 } from "lucide-react";
import { useDartStore } from "@/store/dartStore";
import type { CalibCamInfo, CamState } from "@/lib/dartTypes";

type Opt = {
  redADelta: number;
  greenADelta: number;
  minChroma: number;
  sector20Offset: number;
  autotune: boolean;
  hintX: number; // [0,1], -1 = unset
  hintY: number;
};

const DEFAULT_OPT: Opt = {
  redADelta: 16,
  greenADelta: 12,
  minChroma: 16,
  sector20Offset: 0,
  autotune: false,
  hintX: -1,
  hintY: -1,
};

const SCAN_TIMEOUT_MS = 60000;

const CAM_STATE_LABEL: Record<CamState, string> = {
  warmup: "Apprentissage…",
  normal: "Prête",
  human: "Personne devant",
  clean: "Board vide",
};
const CAM_STATE_COLOR: Record<CamState, string> = {
  warmup: "bg-yellow-500",
  normal: "bg-green-500",
  human: "bg-orange-500",
  clean: "bg-green-500",
};

const FALLBACK_CAMS: CalibCamInfo[] = [0, 1, 2].map((i) => ({
  camId: i,
  calibPath: "",
  zonesPath: "",
  hasCalib: false,
  hasZones: false,
  width: 0,
  height: 0,
  orientationDeg: 0,
  diffThreshold: -1,
}));

export default function CalibrationPage() {
  const connected = useDartStore((s) => s.connected);
  const connect = useDartStore((s) => s.connect);
  const board = useDartStore((s) => s.board);
  const calibration = useDartStore((s) => s.calibration);
  const snapshots = useDartStore((s) => s.snapshots);
  const results = useDartStore((s) => s.autoCalibResults);
  const getCalibration = useDartStore((s) => s.getCalibration);
  const getCameraSnapshot = useDartStore((s) => s.getCameraSnapshot);
  const runAutoCalib = useDartStore((s) => s.runAutoCalib);
  const saveCalibration = useDartStore((s) => s.saveCalibration);

  const [opts, setOpts] = useState<Record<number, Opt>>({});
  const [busy, setBusy] = useState<Record<number, boolean>>({});
  const [timedOut, setTimedOut] = useState<Record<number, boolean>>({});
  const [saving, setSaving] = useState<Record<number, boolean>>({});
  const [savedFlash, setSavedFlash] = useState<Record<number, boolean>>({});
  const [view, setView] = useState<Record<number, "live" | "result">>({});
  const [showCurrentCalib, setShowCurrentCalib] = useState(true);
  const [live, setLive] = useState(true);

  const prevResults = useRef<Record<number, unknown>>({});
  const prevCalib = useRef<unknown>(null);
  const pendingSaves = useRef<Set<number>>(new Set());
  const scanTimers = useRef<Record<number, ReturnType<typeof setTimeout>>>({});

  const getOpt = (map: Record<number, Opt>, camId: number): Opt => map[camId] ?? DEFAULT_OPT;
  const setOpt = (camId: number, patch: Partial<Opt>) =>
    setOpts((o) => ({ ...o, [camId]: { ...getOpt(o, camId), ...patch } }));

  useEffect(() => {
    connect();
  }, [connect]);

  // Initial pull once connected.
  useEffect(() => {
    if (!connected) return;
    getCalibration();
    getCameraSnapshot(undefined, showCurrentCalib);
  }, [connected, getCalibration, getCameraSnapshot, showCurrentCalib]);

  // Live preview polling (keeps running during a scan thanks to async scans).
  useEffect(() => {
    if (!connected || !live) return;
    getCameraSnapshot(undefined, showCurrentCalib);
    const id = setInterval(() => getCameraSnapshot(undefined, showCurrentCalib), 1500);
    return () => clearInterval(id);
  }, [connected, live, showCurrentCalib, getCameraSnapshot]);

  // A fresh scan result clears the busy flag + its timeout, syncs the sliders
  // to the thresholds actually used (auto-tune), and flips the view to it.
  useEffect(() => {
    for (const k of Object.keys(results)) {
      const camId = Number(k);
      const r = results[camId];
      if (r === prevResults.current[camId]) continue;
      if (scanTimers.current[camId]) {
        clearTimeout(scanTimers.current[camId]);
        delete scanTimers.current[camId];
      }
      setBusy((b) => ({ ...b, [camId]: false }));
      setTimedOut((t) => ({ ...t, [camId]: false }));
      if (r?.ok) {
        setOpts((o) => ({
          ...o,
          [camId]: { ...getOpt(o, camId), redADelta: r.redADelta, greenADelta: r.greenADelta, minChroma: r.minChroma },
        }));
        setView((v) => ({ ...v, [camId]: "result" }));
      }
    }
    prevResults.current = { ...results };
  }, [results]);

  // The calibration overview is re-broadcast after a save → flash + show live.
  useEffect(() => {
    if (calibration === prevCalib.current) return;
    prevCalib.current = calibration;
    if (pendingSaves.current.size === 0) return;
    const saved = Array.from(pendingSaves.current);
    pendingSaves.current.clear();
    setSaving({});
    setSavedFlash((f) => {
      const n = { ...f };
      saved.forEach((c) => (n[c] = true));
      return n;
    });
    setView((v) => {
      const n = { ...v };
      saved.forEach((c) => (n[c] = "live")); // applied calib now shows live
      return n;
    });
    const t = setTimeout(() => setSavedFlash({}), 3000);
    return () => clearTimeout(t);
  }, [calibration]);

  const cams = calibration?.cams ?? FALLBACK_CAMS;
  const replay = calibration?.replay ?? false;

  const scan = (camId: number) => {
    const o = getOpt(opts, camId);
    setBusy((b) => ({ ...b, [camId]: true }));
    setTimedOut((t) => ({ ...t, [camId]: false }));
    clearTimeout(scanTimers.current[camId]);
    scanTimers.current[camId] = setTimeout(() => {
      setBusy((b) => ({ ...b, [camId]: false }));
      setTimedOut((t) => ({ ...t, [camId]: true }));
    }, SCAN_TIMEOUT_MS);
    runAutoCalib(camId, {
      redADelta: o.redADelta,
      greenADelta: o.greenADelta,
      minChroma: o.minChroma,
      sector20Offset: o.sector20Offset,
      autotune: o.autotune,
      sector20HintX: o.hintX,
      sector20HintY: o.hintY,
    });
  };

  const save = (camId: number) => {
    pendingSaves.current.add(camId);
    setSaving((s) => ({ ...s, [camId]: true }));
    saveCalibration(camId);
  };

  // Global activity banner.
  const scanningCam = Object.keys(busy).find((k) => busy[Number(k)]);
  const savedCam = Object.keys(savedFlash).find((k) => savedFlash[Number(k)]);
  let banner: { text: string; tone: "info" | "ok" } | null = null;
  if (scanningCam !== undefined) banner = { text: `Scan caméra ${scanningCam} en cours…`, tone: "info" };
  else if (savedCam !== undefined) banner = { text: `Calibration caméra ${savedCam} enregistrée et appliquée`, tone: "ok" };

  return (
    <div className="min-h-screen bg-black text-white p-4">
      <div className="max-w-6xl mx-auto flex flex-col gap-4">
        {/* Header */}
        <div className="flex items-center justify-between flex-wrap gap-3">
          <div className="flex items-center gap-3">
            <Link href="/" className="p-2 rounded-lg bg-gray-900 border border-gray-700 hover:border-red-500">
              <ArrowLeft className="w-5 h-5" />
            </Link>
            <div className="p-3 rounded-full bg-red-600">
              <Crosshair className="w-6 h-6" />
            </div>
            <div>
              <h1 className="text-2xl font-black">Calibration</h1>
              <div className="flex items-center gap-2 text-sm text-gray-400">
                <span className={`h-2.5 w-2.5 rounded-full ${connected ? "bg-green-500" : "bg-red-600"}`} />
                {connected ? "Serveur connecté" : "Serveur hors-ligne"}
                {connected && (
                  <span className="ml-2 rounded bg-gray-800 px-2 py-0.5 text-xs">
                    {replay ? "Mode replay (vidéos)" : "Mode live (caméras)"}
                  </span>
                )}
              </div>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={() => setShowCurrentCalib((v) => !v)}
              className={`flex items-center gap-2 rounded-lg border px-3 py-2.5 active:scale-95 ${
                showCurrentCalib ? "bg-red-600 border-red-500" : "bg-gray-900 border-gray-700 hover:border-red-500"
              }`}
            >
              <Crosshair className="w-4 h-4" />
              Calibration actuelle
            </button>
            <button
              onClick={() => setLive((v) => !v)}
              className="flex items-center gap-2 rounded-lg bg-gray-900 border border-gray-700 px-3 py-2.5 hover:border-red-500 active:scale-95"
            >
              {live ? <Pause className="w-4 h-4" /> : <Play className="w-4 h-4" />}
              {live ? "Aperçu en pause" : "Reprendre"}
            </button>
          </div>
        </div>

        {/* Bannière d'activité */}
        {banner && (
          <div
            className={`flex items-center gap-2 rounded-xl px-4 py-2.5 text-sm font-semibold ${
              banner.tone === "ok" ? "bg-green-700/40 text-green-200" : "bg-blue-700/40 text-blue-100"
            }`}
          >
            {banner.tone === "ok" ? <CheckCircle2 className="w-4 h-4" /> : <Loader2 className="w-4 h-4 animate-spin" />}
            {banner.text}
          </div>
        )}

        {!connected && (
          <div className="rounded-2xl bg-gray-900 border border-gray-700 p-6 text-gray-300">
            En attente du serveur de fléchettes… Vérifie qu&apos;il tourne sur la machine des
            caméras et que l&apos;URL est correcte dans les{" "}
            <Link href="/settings" className="text-red-400 underline">
              réglages
            </Link>
            .
          </div>
        )}

        {/* Caméras */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-4">
          {cams.map((cam) => {
            const camId = cam.camId;
            const o = getOpt(opts, camId);
            const r = results[camId];
            const snap = snapshots[camId];
            const state = board?.cams.find((c) => c.id === camId)?.state;
            const showResult = view[camId] === "result" && r?.ok && r.overlay;
            const imgSrc = showResult
              ? `data:image/jpeg;base64,${r!.overlay}`
              : snap
              ? `data:image/jpeg;base64,${snap}`
              : null;
            const isBusy = !!busy[camId];

            return (
              <div key={camId} className="rounded-2xl bg-gray-900 border border-gray-700 p-4 flex flex-col gap-3">
                {/* Titre + statut */}
                <div className="flex items-center justify-between">
                  <h2 className="text-lg font-bold">Caméra {camId}</h2>
                  {state && (
                    <span className="flex items-center gap-1.5 text-xs text-gray-300">
                      <span className={`h-2 w-2 rounded-full ${CAM_STATE_COLOR[state]}`} />
                      {CAM_STATE_LABEL[state]}
                    </span>
                  )}
                </div>

                {/* Aperçu / overlay */}
                <div
                  onClick={(e) => {
                    const rect = e.currentTarget.getBoundingClientRect();
                    setOpt(camId, {
                      hintX: Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width)),
                      hintY: Math.max(0, Math.min(1, (e.clientY - rect.top) / rect.height)),
                    });
                  }}
                  className="relative aspect-[4/3] w-full overflow-hidden rounded-lg bg-black border border-gray-800 cursor-crosshair select-none"
                >
                  {imgSrc ? (
                    // eslint-disable-next-line @next/next/no-img-element
                    <img src={imgSrc} alt={`cam ${camId}`} className="h-full w-full object-contain" draggable={false} />
                  ) : (
                    <div className="flex h-full items-center justify-center text-sm text-gray-500">
                      {connected ? "Aucune image" : "—"}
                    </div>
                  )}

                  {/* Badge mode d'affichage */}
                  <span className="absolute left-2 top-2 rounded bg-black/70 px-2 py-0.5 text-[10px] text-gray-200">
                    {showResult ? "Résultat du scan" : showCurrentCalib && cam.hasZones ? "Calibration actuelle" : "Live"}
                  </span>

                  {/* Repère secteur 20 */}
                  {o.hintX >= 0 && o.hintY >= 0 && !showResult && (
                    <div
                      className="absolute -translate-x-1/2 -translate-y-1/2 pointer-events-none"
                      style={{ left: `${o.hintX * 100}%`, top: `${o.hintY * 100}%` }}
                    >
                      <Crosshair className="w-7 h-7 text-red-500 drop-shadow" />
                      <span className="absolute left-8 top-0 rounded bg-black/70 px-1 text-[10px] text-red-300">20</span>
                    </div>
                  )}

                  {isBusy && (
                    <div className="absolute inset-0 flex flex-col items-center justify-center gap-2 bg-black/60 text-sm font-semibold">
                      <Loader2 className="w-6 h-6 animate-spin" />
                      Scan en cours…
                    </div>
                  )}

                  {r?.ok && (
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        setView((v) => ({ ...v, [camId]: v[camId] === "result" ? "live" : "result" }));
                      }}
                      className="absolute right-2 top-2 rounded bg-black/70 px-2 py-1 text-[11px] hover:bg-black/90"
                    >
                      {showResult ? "Voir live" : "Voir le scan"}
                    </button>
                  )}
                </div>
                <p className="text-[11px] text-gray-500 -mt-1">
                  Astuce : touche le secteur 20 sur l&apos;image si la numérotation est tournée.
                </p>

                {/* Config dispo */}
                <div className="rounded-lg bg-gray-800/60 p-3 text-xs text-gray-300 space-y-1">
                  <div className="flex justify-between">
                    <span className="text-gray-400">Config</span>
                    <span className="font-mono">{cam.calibPath ? cam.calibPath.split("/").pop() : "—"}</span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-gray-400">Calibrée</span>
                    <span className={cam.hasCalib ? "text-green-400" : "text-red-400"}>{cam.hasCalib ? "oui" : "non"}</span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-gray-400">Zone map</span>
                    <span className={cam.hasZones ? "text-green-400" : "text-red-400"}>{cam.hasZones ? "oui" : "non"}</span>
                  </div>
                  {cam.hasCalib && (
                    <div className="flex justify-between">
                      <span className="text-gray-400">Résolution</span>
                      <span className="font-mono">
                        {cam.width}×{cam.height} · {cam.orientationDeg.toFixed(0)}°
                      </span>
                    </div>
                  )}
                </div>

                {/* Réglages */}
                <div className="space-y-2.5">
                  <Slider label="Rouge a*" value={o.redADelta} min={4} max={60} onChange={(v) => setOpt(camId, { redADelta: v })} />
                  <Slider label="Vert a*" value={o.greenADelta} min={4} max={60} onChange={(v) => setOpt(camId, { greenADelta: v })} />
                  <Slider label="Chroma" value={o.minChroma} min={6} max={60} onChange={(v) => setOpt(camId, { minChroma: v })} />

                  <div className="flex items-center justify-between gap-2">
                    <span className="text-xs text-gray-400">Secteur 20 (rotation)</span>
                    <div className="flex items-center gap-2">
                      <button onClick={() => setOpt(camId, { sector20Offset: o.sector20Offset - 1 })}
                        className="h-9 w-9 rounded bg-gray-700 hover:bg-gray-600 font-bold active:scale-95">−</button>
                      <span className="w-6 text-center font-mono text-sm">{((o.sector20Offset % 20) + 20) % 20}</span>
                      <button onClick={() => setOpt(camId, { sector20Offset: o.sector20Offset + 1 })}
                        className="h-9 w-9 rounded bg-gray-700 hover:bg-gray-600 font-bold active:scale-95">+</button>
                    </div>
                  </div>

                  <div className="flex items-center justify-between">
                    <label className="flex items-center gap-2 text-xs text-gray-300">
                      <input type="checkbox" checked={o.autotune} className="h-4 w-4 accent-red-600"
                        onChange={(e) => setOpt(camId, { autotune: e.target.checked })} />
                      Auto-tune des seuils
                    </label>
                    <button
                      onClick={() => setOpt(camId, { ...DEFAULT_OPT })}
                      className="flex items-center gap-1 text-xs text-gray-400 hover:text-white"
                      title="Réinitialiser les réglages"
                    >
                      <RotateCcw className="w-3.5 h-3.5" /> Reset
                    </button>
                  </div>
                </div>

                {/* Résultat */}
                {timedOut[camId] && (
                  <div className="rounded-lg bg-red-900/30 text-red-200 p-2 text-xs">
                    Pas de réponse du scan (timeout). Réessaie.
                  </div>
                )}
                {r && !isBusy && (
                  <div className={`rounded-lg p-2 text-xs ${r.ok ? "bg-green-900/30 text-green-200" : "bg-red-900/30 text-red-200"}`}>
                    {r.ok ? (
                      <>
                        <div className="flex items-center gap-1 font-semibold">
                          <CheckCircle2 className="w-3.5 h-3.5" /> Scan terminé
                        </div>
                        <div className="mt-0.5">
                          Triples {r.triplesFound}/20 · Doubles {r.doublesFound}/20 · reproj {r.meanReprojErrPx.toFixed(1)}px
                        </div>
                        {r.warning && <div className="text-yellow-300 mt-0.5">⚠ {r.warning}</div>}
                      </>
                    ) : (
                      <div>Échec : {r.error || "scan impossible"}</div>
                    )}
                  </div>
                )}

                {/* Actions */}
                <div className="flex gap-2 mt-auto">
                  <button
                    onClick={() => scan(camId)}
                    disabled={!connected || isBusy}
                    className="flex-1 flex items-center justify-center gap-2 rounded-lg bg-blue-600 hover:bg-blue-700 disabled:opacity-50 px-3 py-3 font-semibold active:scale-95"
                  >
                    {isBusy ? <Loader2 className="w-4 h-4 animate-spin" /> : <ScanLine className="w-4 h-4" />}
                    {isBusy ? "Scan…" : "Scanner"}
                  </button>
                  <button
                    onClick={() => save(camId)}
                    disabled={!r?.ok || !!saving[camId]}
                    className={`flex-1 flex items-center justify-center gap-2 rounded-lg px-3 py-3 font-semibold active:scale-95 disabled:opacity-50 ${
                      savedFlash[camId] ? "bg-green-600" : "bg-red-600 hover:bg-red-700"
                    }`}
                  >
                    {savedFlash[camId] ? <CheckCircle2 className="w-4 h-4" /> : <Save className="w-4 h-4" />}
                    {savedFlash[camId] ? "Enregistré !" : saving[camId] ? "…" : "Enregistrer & utiliser"}
                  </button>
                </div>
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}

function Slider({
  label,
  value,
  min,
  max,
  onChange,
}: {
  label: string;
  value: number;
  min: number;
  max: number;
  onChange: (v: number) => void;
}) {
  return (
    <div className="flex items-center gap-3">
      <span className="w-16 shrink-0 text-xs text-gray-400">{label}</span>
      <input
        type="range"
        min={min}
        max={max}
        value={value}
        onChange={(e) => onChange(Number(e.target.value))}
        className="flex-1 h-2 accent-red-600"
      />
      <span className="w-7 text-right font-mono text-xs">{value}</span>
    </div>
  );
}
