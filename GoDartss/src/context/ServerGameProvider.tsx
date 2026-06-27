"use client";

/**
 * Adaptateur : fournit le MÊME contexte `useGame()` que GameContext, mais
 * adossé au serveur autoritatif (store WS) au lieu d'un reducer local.
 *
 * Objectif : réutiliser tels quels les composants stylés existants
 * (Dartboard, EditableScoreboard, CurrentPlayerDisplay, UndoButton…) en
 * traduisant leurs `dispatch` en commandes serveur. Aucun scoring local :
 * l'état affiché est celui reçu du serveur.
 *
 *   THROW       → manual_throw
 *   EDIT_THROW  → correct_throw
 *   UNDO        → undo
 *   NEXT_PLAYER → next_player
 */

import React from "react";
import { GameCtx } from "@/context/GameContext";
import type { GameState as FrontGameState, Throw } from "@/types/game";
import { useDartStore } from "@/store/dartStore";

type Action =
  | { type: "THROW"; payload: Throw }
  | { type: "EDIT_THROW"; payload: { turnIndex: number; throwIndex: number; newThrow: Throw } }
  | { type: "UNDO" }
  | { type: "NEXT_PLAYER" }
  | { type: string; payload?: unknown };

export function ServerGameProvider({ children }: { children: React.ReactNode }) {
  const game = useDartStore((s) => s.game);
  const connected = useDartStore((s) => s.connected);
  const manualThrow = useDartStore((s) => s.manualThrow);
  const correctThrow = useDartStore((s) => s.correctThrow);
  const undo = useDartStore((s) => s.undo);
  const nextPlayer = useDartStore((s) => s.nextPlayer);

  const dispatch = React.useCallback(
    (action: Action) => {
      switch (action.type) {
        case "THROW": {
          const t = action.payload as Throw;
          manualThrow(t.value, t.multiplier);
          break;
        }
        case "EDIT_THROW": {
          const { turnIndex, throwIndex, newThrow } =
            action.payload as { turnIndex: number; throwIndex: number; newThrow: Throw };
          correctThrow(turnIndex, throwIndex, newThrow.value, newThrow.multiplier);
          break;
        }
        case "UNDO":
          undo();
          break;
        case "NEXT_PLAYER":
          nextPlayer();
          break;
        default:
          break; // SET_WINNER / CLEAR_WINNER : gérés côté serveur
      }
    },
    [manualThrow, correctThrow, undo, nextPlayer]
  );

  if (!game) return <>{children}</>;

  // L'état serveur est structurellement identique au GameState du front.
  const value = {
    state: game as unknown as FrontGameState,
    dispatch,
    gameId: undefined,
    setGameId: () => {},
    isConnected: connected,
    persistenceHooks: undefined,
    badgeHooks: undefined,
  } as unknown as React.ComponentProps<typeof GameCtx.Provider>["value"];

  return <GameCtx.Provider value={value}>{children}</GameCtx.Provider>;
}
