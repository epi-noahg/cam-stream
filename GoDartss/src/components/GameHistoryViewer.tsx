"use client";

import { theme, cn, getCardClasses } from "@/lib/theme";
import { History } from "lucide-react";
import { GameState } from "@/types/game";

interface GameHistoryViewerProps {
  gameState: GameState;
}

/**
 * Composant d'affichage de l'historique en mode read-only
 * N'utilise pas le GameContext pour éviter de modifier l'état de la partie
 */
export default function GameHistoryViewer({ gameState }: GameHistoryViewerProps) {
  
  // Fonction pour formater l'affichage d'un lancer
  const formatThrow = (thr: { bust?: boolean; value: number; multiplier: number }): string => {
    if (thr.bust) return "BUST";
    if (thr.value === 0) return "OUT";
    if (thr.value === 25 || thr.value === 50) return thr.value.toString();
    return `${thr.multiplier === 2 ? "D" : thr.multiplier === 3 ? "T" : ""}${thr.value}`;
  };

  // Calcule le nombre total de manches
  const totalRounds = Math.ceil(gameState.turns.length / gameState.players.length);

  return (
    <div className={cn(
      "w-full h-full flex flex-col",
      getCardClasses('player')
    )}>
      {/* Header compact */}
      <div className="flex items-center gap-2 mb-2 pb-2 border-b border-gray-700 flex-shrink-0">
        <History className={cn("w-4 h-4", theme.text.accent)} />
        <h3 className={cn("text-sm font-bold", theme.text.primary)}>
          Historique de la partie
        </h3>
        <div className={cn("ml-auto text-xs", theme.text.secondary)}>
          {totalRounds} manches • {gameState.players.length} joueurs
        </div>
      </div>

      {/* Tableau d'historique - Joueurs en colonnes (haut), Manches en lignes (gauche) */}
      <div className="flex-1 min-h-0 overflow-auto">
        {totalRounds === 0 || gameState.turns.length === 0 ? (
          <div className="text-center py-4">
            <History className={cn("w-6 h-6 mx-auto mb-1", theme.text.muted)} />
            <p className={cn("text-xs", theme.text.muted)}>
              Aucun historique disponible
            </p>
          </div>
        ) : (
          <div className="overflow-auto">
            {/* Tableau avec manches en lignes et joueurs en colonnes */}
            <div className="min-w-full">
              {/* En-tête du tableau - Noms des joueurs */}
              <div className="grid gap-2 p-3 rounded-t bg-gray-700/30 border-b border-gray-600 text-sm font-semibold sticky top-0 z-10" 
                   style={{gridTemplateColumns: `100px repeat(${gameState.players.length}, minmax(120px, 1fr))`}}>
                <div className={cn("text-left", theme.text.secondary)}>Manche</div>
                {gameState.players.map((player) => (
                  <div key={player.id} className={cn(
                    "text-center font-bold",
                    gameState.winner === player.id ? "text-yellow-400" : theme.text.primary
                  )}>
                    {player.nickname}
                    {gameState.winner === player.id && <span className="text-yellow-500 ml-1">👑</span>}
                    {gameState.finishedPlayers.includes(player.id) && gameState.winner !== player.id && (
                      <span className="text-gray-400 ml-1">🏁</span>
                    )}
                  </div>
                ))}
              </div>
              
              {/* Lignes des manches */}
              <div className="space-y-1">
                {Array.from({length: totalRounds}, (_, roundIndex) => (
                  <div key={roundIndex + 1} 
                       className="grid gap-2 p-2 rounded transition-colors text-sm hover:bg-gray-700/10"
                       style={{gridTemplateColumns: `100px repeat(${gameState.players.length}, minmax(120px, 1fr))`}}>
                    
                    {/* Numéro de manche */}
                    <div className={cn(
                      "flex items-center font-bold text-center", 
                      theme.text.primary
                    )}>
                      Manche {roundIndex + 1}
                    </div>
                    
                    {/* Colonnes des joueurs pour cette manche */}
                    {gameState.players.map((player, playerIndex) => {
                      const turnIndex = roundIndex * gameState.players.length + playerIndex;
                      const turn = gameState.turns[turnIndex] || [];
                      const hasTurn = turnIndex < gameState.turns.length;
                      
                      // Calcule le score après cette manche
                      let scoreAfterRound = gameState.options.startingScore;
                      
                      // Recalcule le score en rejouant tous les tours précédents + le tour actuel
                      for (let i = 0; i <= turnIndex && i < gameState.turns.length; i++) {
                        const currentTurn = gameState.turns[i];
                        const currentPlayerIndex = i % gameState.players.length;
                        
                        if (currentPlayerIndex === playerIndex && currentTurn) {
                          // Calcule le score de ce tour
                          const turnScore = currentTurn.reduce((sum, thr) => {
                            if (thr.bust || thr.value === 0) return sum;
                            const value = thr.value === 25 || thr.value === 50 
                              ? thr.value 
                              : thr.value * thr.multiplier;
                            return sum + value;
                          }, 0);
                          
                          // Si pas de bust, soustraire les points
                          if (!currentTurn.some(t => t.bust)) {
                            scoreAfterRound -= turnScore;
                          }
                        }
                      }
                      
                      // Calcule le score de cette manche spécifique
                      const roundScore = turn.reduce((sum, thr) => {
                        if (thr.bust || thr.value === 0) return sum;
                        const value = thr.value === 25 || thr.value === 50 
                          ? thr.value 
                          : thr.value * thr.multiplier;
                        return sum + value;
                      }, 0);
                      
                      // Formate les lancers
                      const throws = turn.map(formatThrow).join(' ');
                      
                      return (
                        <div key={player.id} className="text-center py-2">
                          {hasTurn && turn.length > 0 ? (
                            <div className="space-y-1">
                              {/* Score après la manche */}
                              <div className={cn(
                                "font-bold text-lg",
                                scoreAfterRound === 0 ? "text-green-400" : theme.text.primary
                              )}>
                                {scoreAfterRound}
                              </div>
                              {/* Points de la manche */}
                              <div className={cn(
                                "text-xs font-medium",
                                roundScore > 0 ? "text-red-400" : theme.text.secondary
                              )}>
                                {roundScore > 0 ? `-${roundScore}` : '0'}
                              </div>
                              {/* Lancers détaillés */}
                              {throws && (
                                <div className={cn("text-xs px-1 py-0.5 rounded bg-gray-800/50 border border-gray-600/30", theme.text.secondary)} 
                                     title={throws}>
                                  {throws}
                                </div>
                              )}
                            </div>
                          ) : (
                            <div className={cn("text-center", theme.text.muted)}>-</div>
                          )}
                        </div>
                      );
                    })}
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}