"use client";

/** Bandeau de statut du board : état des caméras + phase du round. */

import type { BoardStatus } from "@/lib/dartTypes";

const CAM_COLOR: Record<string, string> = {
  normal: "bg-green-500",
  clean: "bg-emerald-400",
  warmup: "bg-yellow-500",
  human: "bg-orange-500",
};

const CAM_LABEL: Record<string, string> = {
  normal: "prête",
  clean: "board vide",
  warmup: "init…",
  human: "main détectée",
};

export default function BoardStatusBar({
  board,
  connected,
}: {
  board: BoardStatus | null;
  connected: boolean;
}) {
  return (
    <div className="flex items-center gap-3 rounded-xl bg-gray-900 border border-gray-700 px-4 py-3">
      <span
        className={`h-3 w-3 rounded-full ${connected ? "bg-green-500" : "bg-red-600"}`}
        title={connected ? "connecté" : "déconnecté"}
      />
      <div className="flex gap-2">
        {(board?.cams ?? [0, 1, 2].map((id) => ({ id, state: "warmup", ready: false }))).map(
          (c) => (
            <div key={c.id} className="flex items-center gap-1.5" title={CAM_LABEL[c.state]}>
              <span className={`h-3 w-3 rounded-full ${CAM_COLOR[c.state] ?? "bg-gray-500"}`} />
              <span className="text-xs text-gray-400">cam{c.id}</span>
            </div>
          )
        )}
      </div>
      <div className="ml-auto flex items-center gap-3">
        <span
          className={`text-sm font-semibold ${
            board?.allReady ? "text-green-400" : "text-yellow-400"
          }`}
        >
          {board?.allReady ? "● Prêt à lancer" : "○ Caméras non prêtes"}
        </span>
        {board?.round?.message && (
          <span className="text-sm text-gray-300">{board.round.message}</span>
        )}
      </div>
    </div>
  );
}
