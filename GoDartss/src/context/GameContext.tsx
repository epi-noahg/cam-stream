"use client";

/**
 * Contexte de jeu partagé.
 *
 * L'ancien GameProvider local (reducer + scoring + persistance Prisma) a été
 * retiré : le jeu est désormais piloté par le serveur autoritatif via
 * `ServerGameProvider` (src/context/ServerGameProvider.tsx), qui fournit ce
 * même `GameCtx`. On ne garde donc ici que le contexte et le hook `useGame`,
 * réutilisés tels quels par les composants stylés (Dartboard, scoreboard…).
 */

import React, { createContext, useContext } from "react";
import { GameState, Throw } from "@/types/game";
import { useGamePersistence } from "@/hooks/useGamePersistence";
import { useBadges } from "@/hooks/useBadges.basic";

export type Action =
  | { type: "THROW"; payload: Throw }
  | { type: "NEXT_PLAYER" }
  | { type: "UNDO" }
  | { type: "SET_WINNER"; payload: number }
  | { type: "EDIT_THROW"; payload: { turnIndex: number; throwIndex: number; newThrow: Throw } }
  | { type: "CLEAR_WINNER" };

export const GameCtx = createContext<{
  state: GameState;
  dispatch: React.Dispatch<Action>;
  gameId?: number;
  setGameId: (id?: number) => void;
  isConnected: boolean;
  persistenceHooks: ReturnType<typeof useGamePersistence>;
  badgeHooks: ReturnType<typeof useBadges>;
} | null>(null);

export const useGame = () => {
  const ctx = useContext(GameCtx);
  if (!ctx) throw new Error("useGame must be inside a game provider");
  return ctx;
};
