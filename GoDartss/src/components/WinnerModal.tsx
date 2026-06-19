"use client";

import { useGame } from "@/context/GameContext";
import { useState, useEffect } from "react";
import { theme, cn } from "@/lib/theme";

export default function WinnerModal() {
  const { state, dispatch } = useGame();
  const [showConfetti, setShowConfetti] = useState(false);
  
  // Find the winner
  const winner = state.winner !== null
    ? state.players.find(p => p.id === state.winner)
    : null;
    
  // Get finished players in order
  const finishedPlayers = state.finishedPlayers.map(id => state.players.find(p => p.id === id)).filter(Boolean);

  // Handle confetti effect
  useEffect(() => {
    if (winner) {
      setShowConfetti(true);
      const timer = setTimeout(() => setShowConfetti(false), 5000);
      return () => clearTimeout(timer);
    }
  }, [winner]);

  if (!winner) return null;

  return (
    <div className={cn(
      "fixed inset-0 flex items-center justify-center z-50",
      "bg-black bg-opacity-50"
    )}>
      {/* Confetti effect */}
      {showConfetti && (
        <div className="absolute inset-0 overflow-hidden">
          {[...Array(150)].map((_, i) => (
            <div
              key={i}
              className="absolute w-2 h-2 rounded-full animate-confetti"
              style={{
                backgroundColor: [`#dc2626`, `#991b1b`, `#7f1d1d`, `#ffffff`, `#f3f4f6`][i % 5],
                left: `${Math.random() * 100}%`,
                top: `${Math.random() * 100}%`,
                animationDelay: `${Math.random() * 2}s`,
                transform: `rotate(${Math.random() * 360}deg)`,
              }}
            />
          ))}
        </div>
      )}
      
      <div className={cn(
        "rounded-xl p-8 max-w-md w-full mx-4 text-center relative z-10 animate-pop-in",
        theme.bg.surfaceDark,
        theme.border.accent,
        "border-2"
      )}>
        <div className="text-6xl mb-4">🏆</div>
        <h2 className={cn("text-3xl font-bold mb-2", theme.text.accent)}>Victoire!</h2>
        <p className={cn("text-xl mb-6", theme.text.secondary)}>
          <span className={cn("font-bold", theme.text.accent)}>{winner.nickname}</span> a gagné la partie!
        </p>
        
        <div className={cn(
          "rounded-lg p-4 mb-6",
          theme.bg.secondary,
          theme.border.primary,
          "border"
        )}>
          <p className={cn("text-lg font-semibold", theme.text.primary)}>Score final:</p>
          <p className={cn("text-2xl font-bold", theme.text.accent)}>{winner.score}</p>
        </div>
        
        {finishedPlayers.length > 0 && (
          <div className={cn(
            "rounded-lg p-4 mb-6",
            theme.bg.mutedDark,
            theme.border.primary,
            "border"
          )}>
            <h3 className={cn("font-bold mb-2", theme.text.primary)}>Classement</h3>
            <ol className={cn("list-decimal list-inside", theme.text.secondary)}>
              {finishedPlayers.map((player, index) => (
                <li key={player?.id} className="font-semibold">
                  {index + 1}. {player?.nickname}
                </li>
              ))}
            </ol>
          </div>
        )}
        
        <div className="flex flex-col sm:flex-row gap-3 justify-center">
          <button
            onClick={() => {
              // Reset game
              window.location.reload();
            }}
            className={cn(
              "px-6 py-3 rounded-lg font-medium transition-colors",
              theme.button.secondary
            )}
          >
            Nouvelle partie
          </button>
          <button
            onClick={() => {
              // Let other players finish
              dispatch({ type: "CLEAR_WINNER" });
            }}
            className={cn(
              "px-6 py-3 rounded-lg font-medium transition-colors",
              theme.button.primary
            )}
          >
            Laisser finir les autres
          </button>
        </div>
      </div>
      
      <style jsx>{`
        @keyframes pop-in {
          0% {
            transform: scale(0.5);
            opacity: 0;
          }
          100% {
            transform: scale(1);
            opacity: 1;
          }
        }
        
        @keyframes confetti-fall {
          0% {
            transform: translateY(-100px) rotate(0deg);
            opacity: 1;
          }
          100% {
            transform: translateY(100vh) rotate(720deg);
            opacity: 0;
          }
        }
        
        .animate-pop-in {
          animation: pop-in 0.5s ease-out;
        }
        
        .animate-confetti {
          animation: confetti-fall 5s linear forwards;
        }
      `}</style>
    </div>
  );
}