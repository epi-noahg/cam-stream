"use client";

import { useGame } from "@/context/GameContext";
import { PlayerState } from "@/types/game";
import { theme, cn } from "@/lib/theme";

export default function WinnerScoreboard() {
  const { state } = useGame();
  
  // Get finished players in order with additional stats
  const finishedPlayersWithStats = state.finishedPlayers.map((id, index) => {
    const player = state.players.find(p => p.id === id);
    if (!player) return null;
    
    // Calculate stats
    const totalThrows = player.throws.length;
    const totalScore = state.options.startingScore - player.score;
    const average = totalThrows > 0 ? (totalScore / totalThrows).toFixed(2) : "0.00";
    
    return {
      ...player,
      position: index + 1,
      totalThrows,
      totalScore,
      average
    };
  }).filter(Boolean) as (PlayerState & { position: number; totalThrows: number; totalScore: number; average: string })[];
  
  if (finishedPlayersWithStats.length === 0) return null;

  return (
    <div className={cn(
      "mb-4 p-4 rounded-xl shadow-sm",
      theme.card.default
    )}>
      <h3 className={cn(
        "font-bold mb-3 text-lg flex items-center",
        theme.text.primary
      )}>
        <span className="text-2xl mr-2">🏆</span>
        Classement des gagnants
      </h3>
      <div className="space-y-2">
        {finishedPlayersWithStats.map((finishedPlayer) => (
          <div 
            key={finishedPlayer.id} 
            className={cn(
              "flex items-center justify-between p-3 rounded-lg shadow-sm",
              finishedPlayer.position === 1 ? theme.card.winner : theme.card.finished
            )}
          >
            <div className="flex items-center">
              <span className={cn(
                "text-lg font-bold mr-3",
                finishedPlayer.position === 1 ? theme.text.accent : theme.text.secondary
              )}>
                {finishedPlayer.position}
              </span>
              <span className={cn(
                "font-semibold",
                theme.text.primary
              )}>
                {finishedPlayer.nickname}
              </span>
              {finishedPlayer.position === 1 && (
                <span className={cn(
                  "ml-2 text-xs px-2 py-1 rounded-full",
                  theme.bg.accent,
                  theme.text.primary
                )}>
                  Champion
                </span>
              )}
            </div>
            <div className={cn(
              "flex space-x-3 text-sm",
              theme.text.secondary
            )}>
              <div className="text-center">
                <div className={cn("font-semibold", theme.text.primary)}>{finishedPlayer.legsWon}</div>
                <div className="text-xs">Legs</div>
              </div>
              <div className="text-center">
                <div className={cn("font-semibold", theme.text.accent)}>{finishedPlayer.totalScore}</div>
                <div className="text-xs">Points</div>
              </div>
              <div className="text-center">
                <div className={cn("font-semibold", theme.text.primary)}>{finishedPlayer.average}</div>
                <div className="text-xs">Moyenne</div>
              </div>
              <div className="text-center">
                <div className={cn("font-semibold", theme.text.primary)}>{finishedPlayer.totalThrows}</div>
                <div className="text-xs">Fléchettes</div>
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}