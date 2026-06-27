"use client";

import { useGame } from "@/context/GameContext";
import { Throw } from "@/types/game";
import { getCheckoutSuggestion } from "@/lib/checkout";
import { theme, cn } from "@/lib/theme";
import React from "react";

/**
 * Scoreboard affichant :
 * • Score restant
 * • Legs gagnés
 * • Lancers du tour en cours pour le joueur actif
 */
export default function Scoreboard() {
  const { state } = useGame();

  const currentTurnMap = new Map<number, Throw[]>();
  const playerCount = state.players.length;
  state.turns.forEach((turn, idx) => {
    const playerIndex = idx % playerCount;
    const playerId = state.players[playerIndex].id;
    currentTurnMap.set(playerId, turn);
  });

  return (
    <>
      <table className="min-w-[260px] border text-sm">
        <thead>
          <tr>
            <th className="border px-2 py-1">Joueur</th>
            <th className="border px-2 py-1 text-right">Score</th>
            <th className="border px-2 py-1 text-right">Legs</th>
            <th className="border px-2 py-1">Tour en cours</th>
          </tr>
        </thead>
        <tbody>
          {state.players.map((p, idx) => {
            const isActive = idx === state.currentIndex;
            const turn = currentTurnMap.get(p.id) || [];
            const darts =
              turn.length
                ? turn
                    .map((t) =>
                      t.bust === true
                        ? "BUST"
                        : t.value === 0
                        ? "OUT"
                        : t.value === 25 || t.value === 50
                        ? t.value.toString()
                        : `${t.value}×${t.multiplier}`
                    )
                    .join(" – ")
                : "";

            return (
              <React.Fragment key={p.id}>
                <tr key={p.id} className={isActive ? "bg-primary/20" : ""}>
                  <td className="border px-2 py-1">{p.nickname}</td>
                  <td className="border px-2 py-1 text-right">{p.score}</td>
                  <td className="border px-2 py-1 text-right">{p.legsWon}</td>
                  <td className="border px-2 py-1">{darts}</td>
                </tr>
                {isActive && (() => {
                  const checkout = getCheckoutSuggestion(p.score, 3 - state.dartIndex, state.options.outType);
                  if (!checkout) return null;
                  return (
                    <tr className={cn(theme.text.muted, "italic text-xs")}>
                      <td colSpan={4} className="border px-2 py-1">
                        {checkout
                          .map((t) =>
                            t.value === 25 || t.value === 50
                              ? t.value
                              : `${t.multiplier === 2 ? "D" : t.multiplier === 3 ? "T" : ""}${t.value}`
                          )
                          .join(" – ")}
                      </td>
                    </tr>
                  );
                })()}
              </React.Fragment>
            );
          })}
        </tbody>
      </table>

      <h2 className="text-lg font-bold mt-4">Historique par manche</h2>
      <div className="mt-2 text-sm w-full">
        <table className="w-full border text-sm">
          <thead>
            <tr>
              <th className="border px-2 py-1 text-left">Manche</th>
              {state.players.map((p) => (
                <th key={p.id} className="border px-2 py-1 text-left">{p.nickname}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {Array.from({ length: Math.ceil(state.turns.length / playerCount) }).map((_, roundIndex) => (
              <tr key={roundIndex}>
                <td className="border px-2 py-1">Manche {roundIndex + 1}</td>
                {state.players.map((_, playerIndex) => {
                  const turn = state.turns[roundIndex * playerCount + playerIndex];
                  return (
                    <td key={playerIndex} className="border px-2 py-1">
                      {turn
                        ? turn
                            .map((t) =>
                              t.value === 0
                                ? "OUT"
                                : t.value === 25 || t.value === 50
                                ? t.value
                                : `${t.value}×${t.multiplier}`
                            )
                            .join(" – ")
                        : ""}
                    </td>
                  );
                })}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </>
  );
}