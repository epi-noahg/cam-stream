"use client";

import { useState, useEffect } from "react";
import Link from "next/link";
import { ArrowLeft, Wifi, Save, RefreshCw, Users, Camera } from "lucide-react";
import { theme, cn } from "@/lib/theme";
import { useDartStore } from "@/store/dartStore";
import { randomGuestName } from "@/lib/guestNames";

const SERVER_URL_KEY = "godarts_server_url";

export default function SettingsPage() {
  const [serverUrl, setServerUrl] = useState("");
  const [saved, setSaved] = useState(false);
  const [isAndroid, setIsAndroid] = useState(false);

  // Gestion des joueurs (DB serveur)
  const connected = useDartStore((s) => s.connected);
  const connect = useDartStore((s) => s.connect);
  const players = useDartStore((s) => s.players);
  const getPlayers = useDartStore((s) => s.getPlayers);
  const createPlayer = useDartStore((s) => s.createPlayer);
  const renamePlayer = useDartStore((s) => s.renamePlayer);
  const deletePlayer = useDartStore((s) => s.deletePlayer);
  const refreshBackground = useDartStore((s) => s.refreshBackground);

  const [newName, setNewName] = useState("");
  const [edit, setEdit] = useState<{ id: number; value: string } | null>(null);
  const [confirmDel, setConfirmDel] = useState<number | null>(null);

  useEffect(() => {
    const stored = localStorage.getItem(SERVER_URL_KEY) || "";
    setServerUrl(stored);
    // Détecte si on est dans l'app Android (Capacitor)
    setIsAndroid(
      typeof window !== "undefined" &&
        (window.location.protocol === "capacitor:" ||
          navigator.userAgent.includes("wv") || // WebView
          (window as any).Capacitor !== undefined)
    );
    connect();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (connected) getPlayers();
  }, [connected, getPlayers]);

  const addPlayer = () => {
    const name =
      newName.trim() ||
      randomGuestName(new Set(players.map((p) => p.nickname.toLowerCase())));
    createPlayer(name);
    setNewName("");
  };

  const pbtn = "min-h-11 px-4 rounded-lg font-semibold active:scale-95";

  const handleSave = () => {
    const url = serverUrl.trim().replace(/\/$/, "");
    localStorage.setItem(SERVER_URL_KEY, url);
    setSaved(true);
    setTimeout(() => setSaved(false), 2000);
  };

  const handleReload = () => {
    const url = localStorage.getItem(SERVER_URL_KEY);
    if (url && typeof window !== "undefined") {
      window.location.href = url;
    }
  };

  return (
    <div className={cn("min-h-screen", theme.bg.primary)}>
      <main className="container mx-auto max-w-2xl py-10 px-4 space-y-8">
        <div className="space-y-4">
          <Link
            href="/"
            className={cn(
              "inline-flex items-center gap-2 text-sm font-medium transition-colors",
              theme.text.secondary,
              "hover:text-red-600"
            )}
          >
            <ArrowLeft className="w-4 h-4" />
            Retour
          </Link>
          <h1 className={cn("text-3xl font-black", theme.text.primary)}>
            Paramètres
          </h1>
        </div>

        {/* Section serveur */}
        <div className={cn("rounded-2xl p-6 space-y-4", theme.card.default)}>
          <div className="flex items-center gap-3">
            <Wifi className={cn("w-5 h-5", theme.text.accent)} />
            <h2 className={cn("text-xl font-bold", theme.text.primary)}>
              Serveur local
            </h2>
          </div>

          <p className={cn("text-sm", theme.text.secondary)}>
            Adresse du serveur GoDarts qui tourne sur la machine connectée aux
            caméras. Ex&nbsp;:{" "}
            <code className="bg-gray-800 px-1 rounded text-red-400">
              http://192.168.1.42:3000
            </code>
          </p>

          <div className="space-y-2">
            <label
              className={cn("block text-sm font-medium", theme.text.secondary)}
            >
              URL du serveur
            </label>
            <input
              type="url"
              value={serverUrl}
              onChange={(e) => setServerUrl(e.target.value)}
              placeholder="http://192.168.1.100:3000"
              className={cn(
                "w-full px-4 py-3 rounded-xl border text-base",
                "bg-gray-800 border-gray-600 text-white placeholder-gray-500",
                "focus:outline-none focus:ring-2 focus:ring-red-500 focus:border-transparent"
              )}
            />
          </div>

          <div className="flex gap-3">
            <button
              onClick={handleSave}
              className={cn(
                "flex items-center gap-2 px-4 py-2 rounded-lg font-medium transition-all",
                saved
                  ? "bg-green-600 text-white"
                  : "bg-red-600 hover:bg-red-700 text-white"
              )}
            >
              <Save className="w-4 h-4" />
              {saved ? "Sauvegardé !" : "Sauvegarder"}
            </button>

            {isAndroid && (
              <button
                onClick={handleReload}
                className={cn(
                  "flex items-center gap-2 px-4 py-2 rounded-lg font-medium transition-all",
                  "bg-gray-700 hover:bg-gray-600 text-white"
                )}
              >
                <RefreshCw className="w-4 h-4" />
                Se connecter
              </button>
            )}
          </div>
        </div>

        {/* Section joueurs */}
        <div className={cn("rounded-2xl p-6 space-y-4", theme.card.default)}>
          <div className="flex items-center gap-3">
            <Users className={cn("w-5 h-5", theme.text.accent)} />
            <h2 className={cn("text-xl font-bold", theme.text.primary)}>Joueurs</h2>
          </div>

          <div className="flex gap-2">
            <input
              value={newName}
              onChange={(e) => setNewName(e.target.value)}
              placeholder="Nom (vide = nom cool aléatoire)"
              className="flex-1 min-h-12 rounded-lg bg-gray-800 border border-gray-600 px-4 text-white placeholder-gray-500"
            />
            <button onClick={addPlayer} className={`${pbtn} bg-green-600 text-white`}>➕ Ajouter</button>
          </div>

          {players.length === 0 ? (
            <p className={cn("text-sm", theme.text.secondary)}>
              Aucun joueur. Ajoute-en un, ou ils seront créés automatiquement en fin de partie.
            </p>
          ) : (
            <div className="flex flex-col gap-2">
              {players.map((p) => (
                <div key={p.id} className="flex items-center gap-2 rounded-lg bg-gray-800 px-3 py-2">
                  {edit?.id === p.id ? (
                    <>
                      <input
                        value={edit.value}
                        onChange={(e) => setEdit({ id: p.id, value: e.target.value })}
                        className="flex-1 min-h-11 rounded-lg bg-gray-700 px-3 text-white"
                        autoFocus
                      />
                      <button
                        onClick={() => { if (edit.value.trim()) renamePlayer(p.id, edit.value.trim()); setEdit(null); }}
                        className={`${pbtn} bg-green-600 text-white`}>✓</button>
                      <button onClick={() => setEdit(null)} className={`${pbtn} bg-gray-600 text-white`}>✗</button>
                    </>
                  ) : (
                    <>
                      <span className="flex-1 text-lg font-semibold text-white">{p.nickname}</span>
                      <span className="text-xs text-gray-400 mr-2">{p.totalWins}V / {p.totalGames}P</span>
                      <button onClick={() => setEdit({ id: p.id, value: p.nickname })}
                        className={`${pbtn} bg-gray-700 text-white`} title="Renommer">✎</button>
                      {confirmDel === p.id ? (
                        <>
                          <button onClick={() => { deletePlayer(p.id); setConfirmDel(null); }}
                            className={`${pbtn} bg-red-600 text-white`}>Confirmer</button>
                          <button onClick={() => setConfirmDel(null)} className={`${pbtn} bg-gray-600 text-white`}>✗</button>
                        </>
                      ) : (
                        <button onClick={() => setConfirmDel(p.id)} className={`${pbtn} bg-gray-700 text-white`} title="Supprimer">🗑</button>
                      )}
                    </>
                  )}
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Section caméras */}
        <div className={cn("rounded-2xl p-6 space-y-3", theme.card.default)}>
          <div className="flex items-center gap-3">
            <Camera className={cn("w-5 h-5", theme.text.accent)} />
            <h2 className={cn("text-xl font-bold", theme.text.primary)}>Caméras</h2>
          </div>
          <p className={cn("text-sm", theme.text.secondary)}>
            En cas de fausses détections (lumière, board déplacé), réapprends le fond.
          </p>
          <button onClick={refreshBackground} className={`${pbtn} bg-blue-600 text-white`}>
            🔄 Rafraîchir le fond des caméras
          </button>
        </div>

        {/* Info */}
        <div className={cn("rounded-2xl p-6 space-y-3", theme.card.default)}>
          <h2 className={cn("text-lg font-bold", theme.text.primary)}>
            Comment ça marche ?
          </h2>
          <ol className={cn("space-y-2 text-sm list-decimal list-inside", theme.text.secondary)}>
            <li>Lance le serveur sur la machine des caméras&nbsp;: <code className="text-red-400">npm run dev</code></li>
            <li>Trouve l'IP de la machine dans les paramètres réseau</li>
            <li>Entre l'URL ici et clique "Se connecter"</li>
            <li>L'app affichera directement le serveur en temps réel</li>
          </ol>
        </div>
      </main>
    </div>
  );
}
