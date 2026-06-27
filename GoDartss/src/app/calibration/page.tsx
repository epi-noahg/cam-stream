"use client";

/**
 * Onglet Calibration — piloté par le serveur de fléchettes autoritatif.
 *
 * Affiche les 3 caméras (aperçu live), les configs disponibles (camN.yml +
 * zone map), permet de relancer un auto-scan (AutoCalibrator), d'ajuster les
 * réglages, puis de sauvegarder le bon résultat — qui est alors écrit sur
 * disque ET appliqué à chaud au pipeline en cours.
 */

import { useEffect, useRef, useState } from "react";
import Link from "next/link";
import { ArrowLeft, Crosshair, ScanLine, Save, RotateCcw, Play, Pause } from "lucide-react";
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
  const [saving, setSaving] = useState<Record<number, boolean>>({});
  const [savedFlash, setSavedFlash] = useState<Record<number, boolean>>({});
  const [showOverlay, setShowOverlay] = useState<Record<number, boolean>>({});
  const [live, setLive] = useState(true);

  const prevResults = useRef<Record<number, unknown>>({});
  const prevCalib = useRef<unknown>(null);
  const pendingSaves = useRef<Set<number>>(new Set());

  const getOpt = (map: Record<number, Opt>, camId: number): Opt => map[camId] ?? DEFAULT_OPT;

  useEffect(() => {
    connect();
  }, [connect]);

  // Initial pull once connected.
  useEffect(() => {
    if (!connected) return;
    getCalibration();
    getCameraSnapshot();
  }, [connected, getCalibration, getCameraSnapshot]);

  // Live preview polling.
  useEffect(() => {
    if (!connected || !live) return;
    const id = setInterval(() => getCameraSnapshot(), 1500);
    return () => clearInterval(id);
  }, [connected, live, getCameraSnapshot]);

  // A fresh auto-scan result clears the busy flag and syncs the sliders to the
  // thresholds actually used (so auto-tune is reflected).
  useEffect(() => {
    setBusy((b) => {
      const next = { ...b };
      for (const k of Object.keys(results)) {
        const camId = Number(k);
        if (results[camId] !== prevResults.current[camId]) next[camId] = false;
      }
      return next;
    });
    setOpts((o) => {
      const next = { ...o };
      for (const k of Object.keys(results)) {
        const camId = Number(k);
        const r = results[camId];
        if (r && r.ok && r !== prevResults.current[camId]) {
          next[camId] = {
            ...getOpt(next, camId),
            redADelta: r.redADelta,
            greenADelta: r.greenADelta,
            minChroma: r.minChroma,
          };
        }
      }
      return next;
    });
    prevResults.current = { ...results };
  }, [results]);

  // The calibration overview is re-broadcast after a save → flash confirmation.
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
    const t = setTimeout(() => setSavedFlash({}), 2500);
    return () => clearTimeout(t);
  }, [calibration]);

  const cams = calibration?.cams ?? FALLBACK_CAMS;
  const replay = calibration?.replay ?? false;

  const setOpt = (camId: number, patch: Partial<Opt>) =>
    setOpts((o) => ({ ...o, [camId]: { ...getOpt(o, camId), ...patch } }));

  const onImageClick = (camId: number, e: React.MouseEvent<HTMLDivElement>) => {
    const rect = e.currentTarget.getBoundingClientRect();
    setOpt(camId, {
      hintX: Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width)),
      hintY: Math.max(0, Math.min(1, (e.clientY - rect.top) / rect.height)),
    });
  };

  const scan = (camId: number) => {
    const o = getOpt(opts, camId);
    setBusy((b) => ({ ...b, [camId]: true }));
    setShowOverlay((s) => ({ ...s, [camId]: true }));
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

  return (
    <div className="min-h-screen bg-black text-white p-4">
      <div className="max-w-6xl mx-auto flex flex-col gap-6">
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
          <button
            onClick={() => setLive((v) => !v)}
            className="flex items-center gap-2 rounded-lg bg-gray-900 border border-gray-700 px-3 py-2 hover:border-red-500 active:scale-95"
          >
            {live ? <Pause className="w-4 h-4" /> : <Play className="w-4 h-4" />}
            {live ? "Aperçu en pause" : "Reprendre l'aperçu"}
          </button>
        </div>

        {!connected && (
          <div className="rounded-2xl bg-gray-900 border border-gray-700 p-6 text-gray-300">
            En attente du serveur de fléchettes… Vérifie qu'il tourne sur la machine
            des caméras et que l'URL est correcte dans les{" "}
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
            const overlayShown = showOverlay[camId] && r?.ok && r.overlay;
            const imgSrc = overlayShown
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
                  onClick={(e) => onImageClick(camId, e)}
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
                  {o.hintX >= 0 && o.hintY >= 0 && (
                    <div
                      className="absolute -translate-x-1/2 -translate-y-1/2 pointer-events-none"
                      style={{ left: `${o.hintX * 100}%`, top: `${o.hintY * 100}%` }}
                    >
                      <Crosshair className="w-6 h-6 text-red-500 drop-shadow" />
                      <span className="absolute left-7 top-0 rounded bg-black/70 px-1 text-[10px] text-red-300">20</span>
                    </div>
                  )}
                  {isBusy && (
                    <div className="absolute inset-0 flex items-center justify-center bg-black/60 text-sm font-semibold">
                      Scan en cours…
                    </div>
                  )}
                  {r?.ok && (
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        setShowOverlay((s) => ({ ...s, [camId]: !s[camId] }));
                      }}
                      className="absolute right-2 top-2 rounded bg-black/70 px-2 py-1 text-[11px] hover:bg-black/90"
                    >
                      {overlayShown ? "Voir live" : "Voir overlay"}
                    </button>
                  )}
                </div>
                <p className="text-[11px] text-gray-500 -mt-1">
                  Astuce : clique dans le secteur 20 si la numérotation est tournée.
                </p>

                {/* Config dispo */}
                <div className="rounded-lg bg-gray-800/60 p-3 text-xs text-gray-300 space-y-1">
                  <div className="flex justify-between">
                    <span className="text-gray-400">Config</span>
                    <span className="font-mono">{cam.calibPath ? cam.calibPath.split("/").pop() : "—"}</span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-gray-400">Calibrée</span>
                    <span className={cam.hasCalib ? "text-green-400" : "text-red-400"}>
                      {cam.hasCalib ? "oui" : "non"}
                    </span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-gray-400">Zone map</span>
                    <span className={cam.hasZones ? "text-green-400" : "text-red-400"}>
                      {cam.hasZones ? "oui" : "non"}
                    </span>
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
                <div className="space-y-2">
                  <Slider label="Rouge a*" value={o.redADelta} min={4} max={60} onChange={(v) => setOpt(camId, { redADelta: v })} />
                  <Slider label="Vert a*" value={o.greenADelta} min={4} max={60} onChange={(v) => setOpt(camId, { greenADelta: v })} />
                  <Slider label="Chroma" value={o.minChroma} min={6} max={60} onChange={(v) => setOpt(camId, { minChroma: v })} />

                  <div className="flex items-center justify-between gap-2">
                    <span className="text-xs text-gray-400">Secteur 20 (rotation)</span>
                    <div className="flex items-center gap-2">
                      <button onClick={() => setOpt(camId, { sector20Offset: o.sector20Offset - 1 })}
                        className="h-7 w-7 rounded bg-gray-700 hover:bg-gray-600 font-bold">−</button>
                      <span className="w-6 text-center font-mono text-sm">{((o.sector20Offset % 20) + 20) % 20}</span>
                      <button onClick={() => setOpt(camId, { sector20Offset: o.sector20Offset + 1 })}
                        className="h-7 w-7 rounded bg-gray-700 hover:bg-gray-600 font-bold">+</button>
                    </div>
                  </div>

                  <div className="flex items-center justify-between">
                    <label className="flex items-center gap-2 text-xs text-gray-300">
                      <input type="checkbox" checked={o.autotune}
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
                {r && (
                  <div className={`rounded-lg p-2 text-xs ${r.ok ? "bg-green-900/30 text-green-200" : "bg-red-900/30 text-red-200"}`}>
                    {r.ok ? (
                      <>
                        <div>
                          Triples {r.triplesFound}/20 · Doubles {r.doublesFound}/20 · reproj{" "}
                          {r.meanReprojErrPx.toFixed(1)}px
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
                    className="flex-1 flex items-center justify-center gap-2 rounded-lg bg-blue-600 hover:bg-blue-700 disabled:opacity-50 px-3 py-2.5 font-semibold active:scale-95"
                  >
                    <ScanLine className="w-4 h-4" />
                    {isBusy ? "Scan…" : "Scanner"}
                  </button>
                  <button
                    onClick={() => save(camId)}
                    disabled={!r?.ok || !!saving[camId]}
                    className={`flex-1 flex items-center justify-center gap-2 rounded-lg px-3 py-2.5 font-semibold active:scale-95 disabled:opacity-50 ${
                      savedFlash[camId] ? "bg-green-600" : "bg-red-600 hover:bg-red-700"
                    }`}
                  >
                    <Save className="w-4 h-4" />
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
        className="flex-1 accent-red-600"
      />
      <span className="w-7 text-right font-mono text-xs">{value}</span>
    </div>
  );
}
