"use client";

import { useGame } from "@/context/GameContext";
import { theme, cn, getStateClasses } from "@/lib/theme";
import { Target, TrendingDown, Home } from "lucide-react";
import { getCheckoutSuggestion } from "@/lib/checkout";
import UndoButton from "@/components/UndoButton";
import Link from "next/link";

/**
 * Composant principal d'affichage du joueur actuel
 * Design "Focus Joueur" avec score géant et informations critiques
 * Optimisé pour usage mobile et lisibilité à distance
 */
export default function CurrentPlayerDisplay() {
  const { state } = useGame();
  
  // Joueur actuel (on affiche toujours pour garder le bouton undo)
  const currentPlayer = state.players[state.currentIndex];
  const isCurrentPlayerActive = currentPlayer && !state.finishedPlayers.includes(currentPlayer.id);
  
  // Si pas de joueur actuel, on affiche quand même les boutons de contrôle
  if (!currentPlayer) {
    return (
      <div className={cn(
        "w-full flex justify-end p-2"
      )}>
        <div className="flex items-center gap-2">
          <UndoButton />
          <Link 
            href="/"
            className={cn(
              "inline-flex items-center gap-1 px-2 py-1 rounded text-xs font-medium transition-colors",
              "bg-gray-800 text-white hover:bg-gray-700"
            )}
          >
            <Home className="w-3 h-3" />
            <span className="hidden sm:inline">Accueil</span>
          </Link>
        </div>
      </div>
    );
  }

  // Calcul des suggestions de checkout (nombre de fléchettes restantes)
  const checkoutSuggestion = getCheckoutSuggestion(currentPlayer.score, 3 - state.dartIndex, state.options.outType);
  
  // Indicateurs visuels du nombre de fléchettes
  const dartsIndicators = Array.from({ length: 3 }, (_, index) => ({
    index,
    used: index < state.dartIndex,
    current: index === state.dartIndex,
    remaining: index > state.dartIndex
  }));

  return (
    <div className={cn(
      "w-full",
      "bg-gradient-to-br from-gray-900 via-gray-800 to-gray-900",
      "border-2 border-red-500/50 rounded-xl p-4 shadow-2xl",
      "relative overflow-hidden"
    )}>
      {/* Effet d'éclairage de fond */}
      <div className="absolute inset-0 bg-gradient-to-br from-red-500/10 via-transparent to-transparent rounded-2xl" />
      <div className="absolute top-0 left-1/2 transform -translate-x-1/2 w-48 h-24 bg-red-500/5 blur-xl rounded-full" />
      
      <div className="relative z-10">
        {/* Header avec nom du joueur et contrôles */}
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-2">
            <div className={cn(
              "inline-flex items-center gap-2 px-4 py-2 rounded-lg",
              getStateClasses('active')
            )}>
              <Target className="w-4 h-4 animate-pulse" />
              <span className="text-lg font-bold">
                Tour de {currentPlayer.nickname}
              </span>
              <div className="w-2 h-2 bg-red-500 rounded-full animate-pulse" />
            </div>
          </div>
          
          {/* Boutons de contrôle */}
          <div className="flex items-center gap-2">
            <UndoButton />
            <Link 
              href="/"
              className={cn(
                "inline-flex items-center gap-1 px-2 py-1 rounded text-xs font-medium transition-colors",
                "bg-gray-800 text-white hover:bg-gray-700"
              )}
            >
              <Home className="w-3 h-3" />
              <span className="hidden sm:inline">Accueil</span>
            </Link>
          </div>
        </div>

        {/* Score géant principal */}
        <div className="text-center mb-4">
          <div className="relative">
            {/* Score principal */}
            <div className={cn(
              "text-5xl md:text-6xl font-black",
              "bg-gradient-to-b from-white to-gray-300 bg-clip-text text-transparent",
              "drop-shadow-2xl mb-1"
            )}>
              {currentPlayer.score}
            </div>
            
            {/* Label descriptif */}
            <div className={cn(
              "text-sm font-medium",
              theme.text.secondary
            )}>
              Points restants
            </div>
            
            {/* Indicateur de progression visuelle */}
            <div className="mt-2 w-full bg-gray-700 rounded-full h-1.5">
              <div 
                className="bg-gradient-to-r from-red-600 to-red-400 h-1.5 rounded-full transition-all duration-500"
                style={{ 
                  width: `${Math.max(5, ((state.options.startingScore - currentPlayer.score) / state.options.startingScore) * 100)}%` 
                }}
              />
            </div>
          </div>
        </div>

        {/* Indicateur de fléchettes */}
        <div className="flex justify-center items-center gap-4 mb-4">
          <span className={cn("text-sm font-medium", theme.text.secondary)}>
            Fléchettes:
          </span>
          <div className="flex gap-2">
            {dartsIndicators.map((dart) => (
              <div
                key={dart.index}
                className={cn(
                  "w-4 h-4 rounded-full transition-all duration-300",
                  dart.used && "bg-red-600 shadow-lg shadow-red-600/50",
                  dart.current && "bg-red-500 animate-pulse ring-2 ring-red-400",
                  dart.remaining && "bg-gray-600 border-2 border-gray-500"
                )}
              />
            ))}
          </div>
          <span className={cn("text-sm", theme.text.secondary)}>
            ({state.dartIndex + 1}/3)
          </span>
        </div>

        {/* Suggestions de checkout */}
        {checkoutSuggestion && (
          <div className={cn(
            "text-center p-4 rounded-xl",
            "bg-gradient-to-r from-red-600/20 to-red-700/20",
            "border border-red-500/30"
          )}>
            <div className="flex items-center justify-center gap-2 mb-2">
              <TrendingDown className="w-5 h-5 text-red-400" />
              <span className={cn("font-semibold", theme.text.primary)}>
                Suggestion de finish
              </span>
            </div>
            <div className={cn(
              "text-lg font-bold",
              theme.text.accent
            )}>
              {checkoutSuggestion.map(thr => 
                thr.value === 25 || thr.value === 50 
                  ? thr.value 
                  : `${thr.multiplier === 2 ? "D" : thr.multiplier === 3 ? "T" : ""}${thr.value}`
              ).join(" → ")}
            </div>
          </div>
        )}

      </div>
    </div>
  );
}