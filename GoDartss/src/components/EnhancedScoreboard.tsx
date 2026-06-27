"use client";

import { useGame } from "@/context/GameContext";
import { theme, cn } from "@/lib/theme";
import EditableScoreboard from "./EditableScoreboard";
import WinnerScoreboard from "./WinnerScoreboard";

export default function EnhancedScoreboard() {
  const { state } = useGame();

  // Check if any player has won
  const hasWinner = state.players.some(p => p.legsWon >= state.options.legs);
  
  // Get active players (not finished)
  const activePlayers = state.players.filter(p => !state.finishedPlayers.includes(p.id));
  const currentPlayer = state.players[state.currentIndex];
  const isCurrentPlayerActive = currentPlayer && !state.finishedPlayers.includes(currentPlayer.id);

  return (
    <div>
      {/* Current player indicator - only show if current player hasn't finished */}
      {isCurrentPlayerActive && (
        <div className={cn(
          "mb-4 p-3 rounded-lg text-center",
          theme.card.active,
          "transition-all duration-300"
        )}>
          <span className={cn("font-bold", theme.text.primary)}>
            Tour de : {currentPlayer.nickname}
          </span>
          <span className={cn(
            "ml-2 inline-block h-3 w-3 rounded-full animate-pulse",
            theme.bg.accent
          )}></span>
        </div>
      )}
      
      {/* Winner indicator - only show if there are still active players */}
      {hasWinner && activePlayers.length > 0 && (
        <div className={cn(
          "mb-4 p-3 rounded-lg text-center animate-pulse",
          theme.card.winner
        )}>
          <span className={cn("font-bold", theme.text.primary)}>
            🏆 {state.finishedPlayers.length} joueur{state.finishedPlayers.length > 1 ? 's ont' : ' a'} terminé! La partie continue pour les autres joueurs.
          </span>
        </div>
      )}
      
      {/* All players have finished */}
      {activePlayers.length === 0 && (
        <div className={cn(
          "mb-4 p-3 rounded-lg text-center",
          theme.state.success,
          theme.text.primary
        )}>
          <span className="font-bold">
            🎉 Partie terminée! Tous les joueurs ont fini.
          </span>
        </div>
      )}
      
      {/* Winner scoreboard with stats */}
      <WinnerScoreboard />
      
      {/* Editable scoreboard */}
      <EditableScoreboard />
    </div>
  );
}