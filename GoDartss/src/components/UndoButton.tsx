"use client";

import { useGame } from "@/context/GameContext";
import { theme, cn } from "@/lib/theme";

export default function UndoButton() {
  const { state, dispatch } = useGame();

  // Check if there are any throws to undo
  const canUndo = () => {
    return state.turns.some(turn => turn.length > 0) ||
           state.players.some(player => player.throws.length > 0);
  };

  const handleUndo = () => {
    if (!canUndo()) return;
    dispatch({ type: "UNDO" });
  };

  return (
    <button
      onClick={handleUndo}
      disabled={!canUndo()}
      className={cn(
        "inline-flex items-center gap-2 px-3 py-2 rounded-lg text-sm font-medium transition-all duration-200",
        canUndo() 
          ? cn(
              theme.bg.accent,
              theme.text.primary,
              "hover:bg-red-700 hover:scale-105 shadow-lg"
            )
          : cn(theme.bg.mutedDark, theme.text.muted, "cursor-not-allowed")
      )}
      title={canUndo() ? "Annuler le dernier lancer" : "Aucun lancer à annuler"}
    >
      <svg
        width="16"
        height="16"
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        strokeWidth="2"
        strokeLinecap="round"
        strokeLinejoin="round"
      >
        <path d="M3 7v6h6"/>
        <path d="M21 17a9 9 0 0 0-9-9 9 9 0 0 0-6 2.3L3 13"/>
      </svg>
      Annuler
    </button>
  );
}