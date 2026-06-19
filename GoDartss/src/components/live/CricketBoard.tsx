"use client";

/**
 * Tableau Cricket — grille classique des cibles (20→15 + Bull) par joueur.
 * Lecture seule : l'état vient du serveur autoritatif. Les marques sont rendues
 * comme au tableau noir : / (1), X (2), Ⓧ fermé (3).
 */

import type { GameState } from "@/lib/dartTypes";

const TARGETS = [20, 19, 18, 17, 16, 15, 25];
const LABELS = ["20", "19", "18", "17", "16", "15", "B"];

function markGlyph(m: number) {
  if (m >= 3) return "Ⓧ";
  if (m === 2) return "✕";
  if (m === 1) return "╱";
  return "";
}

export default function CricketBoard({ game }: { game: GameState }) {
  const players = game.players;
  const useBull = game.options.useBull !== false;
  const rows = useBull ? 7 : 6;
  const cols = `minmax(0,2.5rem) repeat(${players.length}, minmax(0,1fr))`;

  return (
    <div className="rounded-lg bg-gray-800/60 p-2">
      {/* En-tête : pseudos + points */}
      <div className="grid items-end gap-1 mb-1" style={{ gridTemplateColumns: cols }}>
        <div className="text-[10px] uppercase tracking-wider text-gray-500">Cible</div>
        {players.map((p, i) => {
          const active = i === game.currentIndex;
          const finished = game.finishedPlayers.includes(p.id);
          return (
            <div key={p.id} className={`text-center rounded-md px-1 py-0.5 ${active ? "bg-red-600/20 border border-red-600" : ""}`}>
              <div className="text-xs font-semibold truncate flex items-center justify-center gap-1">
                {active && <span className="text-red-500">▶</span>}
                {p.nickname}
                {finished && <span className="text-green-400">✓</span>}
              </div>
              <div className="font-mono font-black text-lg">{p.score}</div>
            </div>
          );
        })}
      </div>

      {/* Lignes par cible */}
      <div className="flex flex-col gap-0.5">
        {Array.from({ length: rows }).map((_, r) => (
          <div key={r} className="grid items-center gap-1" style={{ gridTemplateColumns: cols }}>
            <div className="text-center font-bold text-gray-300">{LABELS[r]}</div>
            {players.map((p) => {
              const m = p.marks?.[r] ?? 0;
              const closed = m >= 3;
              return (
                <div key={p.id}
                  className={`text-center font-mono text-xl leading-none py-1 rounded ${closed ? "text-green-400" : "text-white"}`}>
                  {markGlyph(m) || <span className="text-gray-700">·</span>}
                </div>
              );
            })}
          </div>
        ))}
      </div>
    </div>
  );
}
