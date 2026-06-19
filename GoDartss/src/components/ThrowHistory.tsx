"use client";

import { useGame } from "@/context/GameContext";
import { theme, cn, getCardClasses } from "@/lib/theme";
import { History, Edit3, Check, X, Trash2 } from "lucide-react";
import { useState } from "react";
import { Throw } from "@/types/game";

/**
 * Composant d'historique des fléchettes avec édition en ligne
 * Permet de modifier ou supprimer des lancers pour corriger les erreurs
 */
export default function ThrowHistory() {
  const { state, dispatch } = useGame();
  const [editingThrow, setEditingThrow] = useState<{
    turnIndex: number;
    throwIndex: number;
  } | null>(null);
  const [editValue, setEditValue] = useState("");

  // Fonction pour formater l'affichage d'un lancer
  const formatThrow = (thr: Throw): string => {
    if (thr.bust) return "BUST";
    if (thr.value === 0) return "OUT";
    if (thr.value === 25 || thr.value === 50) return thr.value.toString();
    return `${thr.multiplier === 2 ? "D" : thr.multiplier === 3 ? "T" : ""}${thr.value}`;
  };

  // Parse une chaîne en objet Throw
  const parseThrow = (input: string): Throw | null => {
    const trimmed = input.trim().toUpperCase();
    
    if (trimmed === "BUST") return { value: 0, multiplier: 1, bust: true };
    if (trimmed === "OUT") return { value: 0, multiplier: 1 };
    if (trimmed === "MISS" || trimmed === "0") return { value: 0, multiplier: 1 };
    
    // Bull's eye
    if (trimmed === "25") return { value: 25, multiplier: 1 };
    if (trimmed === "50") return { value: 50, multiplier: 1 };
    
    // Double (D20) ou Triple (T20)
    const doubleMatch = trimmed.match(/^D(\d+)$/);
    if (doubleMatch) {
      const value = parseInt(doubleMatch[1]);
      if (value >= 1 && value <= 20) return { value, multiplier: 2 };
    }
    
    const tripleMatch = trimmed.match(/^T(\d+)$/);
    if (tripleMatch) {
      const value = parseInt(tripleMatch[1]);
      if (value >= 1 && value <= 20) return { value, multiplier: 3 };
    }
    
    // Simple (juste le numéro)
    const simpleValue = parseInt(trimmed);
    if (simpleValue >= 1 && simpleValue <= 20) {
      return { value: simpleValue, multiplier: 1 };
    }
    
    return null;
  };

  // Démarre l'édition d'un lancer
  const startEdit = (turnIndex: number, throwIndex: number) => {
    const turn = state.turns[turnIndex];
    if (turn && turn[throwIndex]) {
      const thr = turn[throwIndex];
      setEditValue(formatThrow(thr));
      setEditingThrow({ turnIndex, throwIndex });
    }
  };

  // Confirme l'édition
  const confirmEdit = () => {
    if (!editingThrow) return;
    
    const newThrow = parseThrow(editValue);
    if (newThrow) {
      dispatch({
        type: "EDIT_THROW",
        payload: {
          turnIndex: editingThrow.turnIndex,
          throwIndex: editingThrow.throwIndex,
          newThrow
        }
      });
    }
    
    setEditingThrow(null);
    setEditValue("");
  };

  // Annule l'édition
  const cancelEdit = () => {
    setEditingThrow(null);
    setEditValue("");
  };

  // Supprime un lancer (le remplace par MISS)
  const deleteThrow = (turnIndex: number, throwIndex: number) => {
    dispatch({
      type: "EDIT_THROW",
      payload: {
        turnIndex,
        throwIndex,
        newThrow: { value: 0, multiplier: 1 }
      }
    });
  };

  // Calcule les statistiques
  const totalThrows = state.turns.reduce((total, turn) => total + turn.length, 0);
  const nonZeroThrows = state.turns.reduce((total, turn) => 
    total + turn.filter(t => t.value > 0 && !t.bust).length, 0
  );

  // Calcule le nombre total de manches
  const totalRounds = Math.ceil(state.turns.length / state.players.length);

  return (
    <div className={cn(
      "w-full h-full flex flex-col",
      getCardClasses('player')
    )}>
      {/* Header compact */}
      <div className="flex items-center gap-2 mb-2 pb-2 border-b border-gray-700 flex-shrink-0">
        <History className={cn("w-4 h-4", theme.text.accent)} />
        <h3 className={cn("text-sm font-bold", theme.text.primary)}>
          Manches
        </h3>
        <div className={cn("ml-auto text-xs", theme.text.secondary)}>
          {totalRounds} manches
        </div>
      </div>


      {/* Tableau d'historique - Joueurs en colonnes (haut), Manches en lignes (gauche) */}
      <div className="flex-1 min-h-0 overflow-auto">
        {totalRounds === 0 || state.turns.length === 0 ? (
          <div className="text-center py-4">
            <History className={cn("w-6 h-6 mx-auto mb-1", theme.text.muted)} />
            <p className={cn("text-xs", theme.text.muted)}>
              Aucun historique
            </p>
          </div>
        ) : (
          <div className="overflow-auto">
            {/* Tableau avec manches en lignes et joueurs en colonnes */}
            <div className="min-w-full">
              {/* En-tête du tableau - Noms des joueurs */}
              <div className="grid gap-2 p-3 rounded-t bg-gray-700/30 border-b border-gray-600 text-sm font-semibold sticky top-0 z-10" 
                   style={{gridTemplateColumns: `100px repeat(${state.players.length}, minmax(120px, 1fr))`}}>
                <div className={cn("text-left", theme.text.secondary)}>Manche</div>
                {state.players.map((player, index) => (
                  <div key={player.id} className={cn(
                    "text-center font-bold",
                    index === state.currentIndex ? theme.text.accent : theme.text.primary,
                    state.finishedPlayers.includes(player.id) && "text-yellow-400"
                  )}>
                    {player.nickname}
                    {index === state.currentIndex && <span className="text-red-500 ml-1">▶</span>}
                    {state.finishedPlayers.includes(player.id) && <span className="text-yellow-500 ml-1">🏆</span>}
                  </div>
                ))}
              </div>
              
              {/* Lignes des manches */}
              <div className="space-y-1">
                {Array.from({length: totalRounds}, (_, roundIndex) => (
                  <div key={roundIndex + 1} 
                       className="grid gap-2 p-2 rounded transition-colors text-sm hover:bg-gray-700/10"
                       style={{gridTemplateColumns: `100px repeat(${state.players.length}, minmax(120px, 1fr))`}}>
                    
                    {/* Numéro de manche */}
                    <div className={cn(
                      "flex items-center font-bold text-center", 
                      theme.text.primary
                    )}>
                      Manche {roundIndex + 1}
                    </div>
                    
                    {/* Colonnes des joueurs pour cette manche */}
                    {state.players.map((player, playerIndex) => {
                      const turnIndex = roundIndex * state.players.length + playerIndex;
                      const turn = state.turns[turnIndex] || [];
                      const hasTurn = turnIndex < state.turns.length;
                      
                      // Calcule le score après cette manche
                      let scoreAfterRound = state.options.startingScore;
                      
                      // Recalcule le score en rejouant tous les tours précédents + le tour actuel
                      for (let i = 0; i <= turnIndex && i < state.turns.length; i++) {
                        const currentTurn = state.turns[i];
                        const currentPlayerIndex = i % state.players.length;
                        
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