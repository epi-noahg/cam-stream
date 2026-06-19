"use client";

/**
 * Accueil / menu — hub serveur (Ordi + Tablette, même DB).
 * Sélection du mode de jeu + accès Classement / Réglages + sélecteur d'affichage.
 */

import Link from "next/link";
import { useEffect } from "react";
import { useRouter } from "next/navigation";
import { Target, TrendingUp, Clock, Trophy, Settings, RotateCcw } from "lucide-react";
import { useDartStore } from "@/store/dartStore";

const gameModes = [
  {
    title: "X01",
    description: "301 / 501 / 701 — jusqu'à 0, double-out ou simple-out.",
    icon: TrendingUp,
    href: "/live?new=1",
    available: true,
  },
  {
    title: "Cricket",
    description: "Fermez 15-20 + 25 avant l'adversaire.",
    icon: Target,
    href: "/live?new=1&mode=CRICKET",
    available: true,
  },
  {
    title: "Around the Clock",
    description: "Touchez chaque cible de 1 à 20 dans l'ordre.",
    icon: Clock,
    href: "/live?new=1&mode=ROUND_THE_CLOCK",
    available: true,
  },
];

export default function HomePage() {
  const router = useRouter();
  const connected = useDartStore((s) => s.connected);
  const connect = useDartStore((s) => s.connect);
  const savedGames = useDartStore((s) => s.savedGames);
  const getSavedGames = useDartStore((s) => s.getSavedGames);
  const resumeGame = useDartStore((s) => s.resumeGame);

  useEffect(() => {
    connect();
  }, [connect]);

  useEffect(() => {
    if (connected) getSavedGames();
  }, [connected, getSavedGames]);

  const resume = (id: string) => {
    resumeGame(id);
    router.push("/live");
  };

  return (
    <div className="min-h-screen bg-black text-white p-4">
      <div className="max-w-4xl mx-auto flex flex-col gap-6">
        {/* Header */}
        <div className="flex items-center gap-3">
          <div className="p-3 rounded-full bg-red-600">
            <Target className="w-7 h-7" />
          </div>
          <div>
            <h1 className="text-3xl font-black">GoDarts</h1>
            <div className="flex items-center gap-2 text-sm text-gray-400">
              <span className={`h-2.5 w-2.5 rounded-full ${connected ? "bg-green-500" : "bg-red-600"}`} />
              {connected ? "Serveur connecté" : "Serveur hors-ligne"}
            </div>
          </div>
        </div>

        {/* Modes de jeu */}
        <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
          {gameModes.map((m) => {
            const Icon = m.icon;
            const inner = (
              <div
                className={`h-full rounded-2xl border p-5 flex flex-col gap-3 transition-transform ${
                  m.available
                    ? "bg-gray-900 border-gray-700 hover:border-red-500 active:scale-[0.98] cursor-pointer"
                    : "bg-gray-900/50 border-gray-800 opacity-60"
                }`}
              >
                <Icon className={`w-8 h-8 ${m.available ? "text-red-500" : "text-gray-500"}`} />
                <div className="text-xl font-bold">{m.title}</div>
                <div className="text-sm text-gray-400 flex-1">{m.description}</div>
                {!m.available && <span className="text-xs text-gray-500">Bientôt disponible</span>}
              </div>
            );
            return m.available ? (
              <Link key={m.title} href={m.href}>{inner}</Link>
            ) : (
              <div key={m.title}>{inner}</div>
            );
          })}
        </div>

        {/* Reprendre une partie en cours */}
        {savedGames.length > 0 && (
          <div className="rounded-2xl bg-gray-900 border border-gray-700 p-4">
            <div className="flex items-center gap-2 mb-3">
              <RotateCcw className="w-5 h-5 text-red-500" />
              <h2 className="text-lg font-bold">Reprendre une partie</h2>
            </div>
            <div className="flex flex-col gap-2">
              {savedGames.map((g) => (
                <button key={g.id} onClick={() => resume(g.id)}
                  className="flex items-center justify-between rounded-lg bg-gray-800 hover:border-red-500 border border-transparent px-4 py-3 active:scale-[0.99]">
                  <div className="text-left">
                    <div className="font-semibold">{g.players || "Partie"}</div>
                    <div className="text-xs text-gray-400">{g.startingScore > 0 ? `${g.startingScore} · ` : ""}{g.updatedAt}</div>
                  </div>
                  <span className="text-red-400 font-bold">Continuer →</span>
                </button>
              ))}
            </div>
          </div>
        )}

        {/* Accès rapides */}
        <div className="grid grid-cols-2 gap-4">
          <Link href="/leaderboard"
            className="rounded-2xl bg-gray-900 border border-gray-700 p-5 flex items-center gap-3 hover:border-red-500 active:scale-[0.98]">
            <Trophy className="w-7 h-7 text-yellow-400" />
            <div>
              <div className="text-lg font-bold">Classement & Historique</div>
              <div className="text-sm text-gray-400">Stats des joueurs, parties récentes</div>
            </div>
          </Link>
          <Link href="/settings"
            className="rounded-2xl bg-gray-900 border border-gray-700 p-5 flex items-center gap-3 hover:border-red-500 active:scale-[0.98]">
            <Settings className="w-7 h-7 text-gray-300" />
            <div>
              <div className="text-lg font-bold">Réglages</div>
              <div className="text-sm text-gray-400">Gérer les joueurs, caméras</div>
            </div>
          </Link>
        </div>
      </div>
    </div>
  );
}
