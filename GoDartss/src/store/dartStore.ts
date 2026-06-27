/**
 * Store Zustand + client WebSocket vers le serveur de fléchettes autoritatif.
 *
 * Le serveur C++ est la seule source de vérité : ce store ne fait que refléter
 * l'état reçu (game_state / board_status / dart_detected) et envoyer des
 * commandes/corrections.  Aucune logique de scoring côté front.
 */

"use client";

import { create } from "zustand";
import type {
  BoardStatus,
  Command,
  DartDetected,
  GameState,
  GameSummary,
  LeaderboardRow,
  PlayerRow,
  SavedGame,
  Throw,
} from "@/lib/dartTypes";

/** Résout l'URL du serveur : env, sinon même hôte que le front sur :8080. */
function defaultUrl(): string {
  const env = process.env.NEXT_PUBLIC_DART_WS_URL;
  if (env) return env;
  if (typeof window !== "undefined") {
    return `ws://${window.location.hostname}:8080`;
  }
  return "ws://localhost:8080";
}

type DartStore = {
  connected: boolean;
  game: GameState | null;
  gameId: string | null;
  checkout: Throw[] | null;
  board: BoardStatus | null;
  lastDetected: DartDetected | null;
  leaderboard: LeaderboardRow[];
  history: GameSummary[];
  players: PlayerRow[];
  savedGames: SavedGame[];

  connect: (url?: string) => void;
  disconnect: () => void;
  send: (cmd: Command) => void;

  // raccourcis de commandes
  createGame: (players: { nickname: string; team?: number }[], options: Partial<GameState["options"]>) => void;
  manualThrow: (value: number, multiplier: number) => void;
  correctThrow: (turnIndex: number, throwIndex: number, value: number, multiplier: number) => void;
  clearBoard: () => void;
  undo: () => void;
  nextPlayer: () => void;
  refreshBackground: () => void;
  getLeaderboard: () => void;
  getHistory: () => void;
  getPlayers: () => void;
  createPlayer: (nickname: string) => void;
  renamePlayer: (id: number, nickname: string) => void;
  deletePlayer: (id: number) => void;
  getSavedGames: () => void;
  resumeGame: (id: string) => void;
};

let ws: WebSocket | null = null;
let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
let manualClose = false;

export const useDartStore = create<DartStore>((set, get) => ({
  connected: false,
  game: null,
  gameId: null,
  checkout: null,
  board: null,
  lastDetected: null,
  leaderboard: [],
  history: [],
  players: [],
  savedGames: [],

  connect: (url) => {
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING))
      return;
    manualClose = false;
    const target = url ?? defaultUrl();

    const open = () => {
      ws = new WebSocket(target);
      ws.onopen = () => set({ connected: true });
      ws.onclose = () => {
        set({ connected: false });
        if (!manualClose) {
          if (reconnectTimer) clearTimeout(reconnectTimer);
          reconnectTimer = setTimeout(open, 1500);
        }
      };
      ws.onerror = () => ws?.close();
      ws.onmessage = (ev) => {
        let msg: Record<string, unknown>;
        try { msg = JSON.parse(ev.data as string); } catch { return; }
        switch (msg.type) {
          case "game_state":
            set({ game: msg.state as GameState, gameId: (msg.id as string) ?? null, checkout: (msg.checkout as Throw[]) ?? null });
            break;
          case "board_status":
            set({ board: msg as unknown as BoardStatus });
            break;
          case "dart_detected":
            // Throwing the next dart implies the previous one was accepted; no
            // blocking review prompt. Corrections happen after the fact by
            // tapping a throw in the scoreboard. We just remember the last one.
            set({ lastDetected: msg as unknown as DartDetected });
            break;
          case "leaderboard":
            set({ leaderboard: (msg.leaderboard as LeaderboardRow[]) ?? [] });
            break;
          case "history":
            set({ history: (msg.games as GameSummary[]) ?? [] });
            break;
          case "players":
            set({ players: (msg.players as PlayerRow[]) ?? [] });
            break;
          case "saved_games":
            set({ savedGames: (msg.games as SavedGame[]) ?? [] });
            break;
          default:
            break; // ack / error / players / history handled elsewhere
        }
      };
    };
    open();
  },

  disconnect: () => {
    manualClose = true;
    if (reconnectTimer) clearTimeout(reconnectTimer);
    ws?.close();
    ws = null;
    set({ connected: false });
  },

  send: (cmd) => {
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(cmd));
  },

  createGame: (players, options) =>
    get().send({ type: "create_game", players, options }),
  manualThrow: (value, multiplier) =>
    get().send({ type: "manual_throw", value, multiplier }),
  correctThrow: (turnIndex, throwIndex, value, multiplier) =>
    get().send({ type: "correct_throw", turnIndex, throwIndex, value, multiplier }),
  clearBoard: () => get().send({ type: "clear_board" }),
  undo: () => get().send({ type: "undo" }),
  nextPlayer: () => get().send({ type: "next_player" }),
  refreshBackground: () => get().send({ type: "refresh_background" }),
  getLeaderboard: () => get().send({ type: "get_leaderboard", limit: 10 }),
  getHistory: () => get().send({ type: "get_history", limit: 20 }),
  getPlayers: () => get().send({ type: "get_players" }),
  createPlayer: (nickname) => get().send({ type: "create_player", nickname }),
  renamePlayer: (id, nickname) => get().send({ type: "rename_player", id, nickname }),
  deletePlayer: (id) => get().send({ type: "delete_player", id }),
  getSavedGames: () => get().send({ type: "get_saved_games" }),
  resumeGame: (id) => get().send({ type: "resume_game", id }),
}));
