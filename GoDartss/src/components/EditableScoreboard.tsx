"use client";

import { useGame } from "@/context/GameContext";
import { Throw } from "@/types/game";
import { getCheckoutSuggestion } from "@/lib/checkout";
import { theme, cn } from "@/lib/theme";
import EditableThrow from "./EditableThrow";
import React from "react";

/**
 * Scoreboard affichant :
 * • Score restant
 * • Legs gagnés
 * • Lancers du tour en cours pour le joueur actif (éditables)
 */
export default function EditableScoreboard() {
  const { state, dispatch } = useGame();

  // Create a more accurate mapping of current turns for all active players
  const currentTurnMap = new Map<number, Throw[]>();
  const playerCount = state.players.length;
  
  // Map the last turn for each player
  state.players.forEach((player, playerIndex) => {
    // Find the most recent turn for this player
    for (let i = state.turns.length - 1; i >= 0; i--) {
      const turnPlayerIndex = i % playerCount;
      if (turnPlayerIndex === playerIndex && state.turns[i].length > 0) {
        currentTurnMap.set(player.id, state.turns[i]);
        break;
      }
    }
    
    // If it's the current player's turn and they have an empty turn, include it
    if (playerIndex === state.currentIndex && state.turns.length > 0) {
      const lastTurn = state.turns[state.turns.length - 1];
      if (lastTurn.length === 0 || (state.turns.length - 1) % playerCount === playerIndex) {
        currentTurnMap.set(player.id, lastTurn);
      }
    }
  });

  // Filter out players who have already finished
  const activePlayers = state.players.filter(p => !state.finishedPlayers.includes(p.id));
  
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
          {activePlayers.map((p) => {
            const idx = state.players.findIndex(player => player.id === p.id);
            const isActive = idx === state.currentIndex;
            const turn = currentTurnMap.get(p.id) || [];

            return (
              <React.Fragment key={p.id}>
                <tr
                  key={p.id}
                  className={isActive ? theme.card.active : ""}
                >
                  <td className="border px-2 py-1 font-semibold">
                    {p.nickname}
                  </td>
                  <td className="border px-2 py-1 text-right">{p.score}</td>
                  <td className="border px-2 py-1 text-right">{p.legsWon}</td>
                  <td className="border px-2 py-1">
                    <div className="flex flex-wrap gap-1 items-center">
                      {turn.map((throw_, throwIndex) => {
                        const turnIndex = state.turns.findIndex(t => t === turn);
                        return (
                          <React.Fragment key={throwIndex}>
                            <EditableThrow
                              throw_={throw_}
                              onEdit={(newThrow) => {
                                dispatch({
                                  type: "EDIT_THROW",
                                  payload: { turnIndex, throwIndex, newThrow }
                                });
                              }}
                              isEditable={!state.finishedPlayers.includes(p.id)}
                            />
                            {throwIndex < turn.length - 1 && <span className={theme.text.muted}>–</span>}
                          </React.Fragment>
                        );
                      })}
                    </div>
                  </td>
                </tr>
                {isActive && (() => {
                  const checkout = getCheckoutSuggestion(p.score, 3 - state.dartIndex, state.options.outType);
                  if (!checkout) return null;
                  return (
                    <tr className={cn("italic text-xs", theme.text.secondary)}>
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
                <th key={p.id} className="border px-2 py-1 text-left">
                  {p.nickname}
                  {state.finishedPlayers.includes(p.id) && <span className="ml-1 text-yellow-600">🏆</span>}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {Array.from({ length: Math.ceil(state.turns.length / playerCount) }).map((_, roundIndex) => {
              
              return (
                <tr key={roundIndex}>
                  <td className="border px-2 py-1">Manche {roundIndex + 1}</td>
                  {state.players.map((player, playerIndex) => {
                    const turnIndex = roundIndex * playerCount + playerIndex;
                    const turn = state.turns[turnIndex];
                    const isWinner = state.finishedPlayers.includes(player.id);
                    
                    // Check if this player had already won before this round
                    let hadWonBefore = false;
                    for (let i = 0; i < turnIndex; i++) {
                      const checkIndex = i % playerCount;
                      if (checkIndex === playerIndex) {
                        const checkTurn = state.turns[i];
                        if (checkTurn && checkTurn.length > 0) {
                          // Check if player won in this turn
                          const playerAtTurn = state.players[checkIndex];
                          if (playerAtTurn && state.finishedPlayers.includes(playerAtTurn.id)) {
                            // This is approximate - ideally we'd check the exact turn they won
                            hadWonBefore = true;
                            break;
                          }
                        }
                      }
                    }
                    
                    return (
                      <td key={player.id} className={cn(
                        "border px-2 py-1",
                        isWinner ? theme.card.winner : ""
                      )}>
                        {hadWonBefore ? (
                          <span className={cn("italic", theme.text.muted)}>-</span>
                        ) : (
                          <div className="flex flex-wrap gap-1 items-center">
                            {turn ? turn.map((throw_, throwIdx) => (
                              <React.Fragment key={throwIdx}>
                                <EditableThrow
                                  throw_={throw_}
                                  onEdit={(newThrow) => {
                                    dispatch({
                                      type: "EDIT_THROW",
                                      payload: { turnIndex, throwIndex: throwIdx, newThrow }
                                    });
                                  }}
                                  isEditable={!isWinner}
                                />
                                {throwIdx < turn.length - 1 && <span className={theme.text.muted}>–</span>}
                              </React.Fragment>
                            )) : ""}
                          </div>
                        )}
                      </td>
                    );
                  })}
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </>
  );
}