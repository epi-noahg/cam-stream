"use client";

/**
 * Tableau Around the Clock — cible courante de chaque joueur (1→20).
 * Lecture seule : l'état vient du serveur autoritatif.
 */

import type { GameState } from "@/lib/dartTypes";

export default function RoundClockBoard({ game }: { game: GameState }) {
  return (
    <div className="flex flex-col gap-1.5">
      {game.players.map((p, i) => {
        const active = i === game.currentIndex;
        const finished = game.finishedPlayers.includes(p.id);
        const target = p.target ?? 1;
        const done = target > 20;
        const progress = Math.min(20, Math.max(0, target - 1));
        return (
          <div key={p.id}
            className={`flex items-center justify-between rounded-lg px-3 py-1.5 ${active ? "bg-red-600/20 border border-red-600" : "bg-gray-800"}`}>
            <span className="flex items-center gap-2 text-lg font-semibold truncate">
              {active && <span className="text-red-500">▶</span>}
              {p.nickname}
              {finished && <span className="text-xs text-green-400">✓</span>}
              <span className="text-xs text-gray-400">{progress}/20</span>
            </span>
            <span className={`font-mono font-black ${active ? "text-4xl" : "text-2xl text-gray-300"}`}>
              {done ? "🏁" : `→ ${target}`}
            </span>
          </div>
        );
      })}
    </div>
  );
}
