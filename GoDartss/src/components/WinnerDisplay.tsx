"use client";

import { useGame } from "@/context/GameContext";
import { useState, useEffect } from "react";
import { theme, cn } from "@/lib/theme";

export default function WinnerDisplay() {
  const { state } = useGame();
  const [showAnimation, setShowAnimation] = useState(false);
  const [showButtons, setShowButtons] = useState(false);
  
  // Find the winner
  const winner = state.winner !== null
    ? state.players.find(p => p.id === state.winner)
    : null;

  // Handle victory animation sequence
  useEffect(() => {
    if (winner) {
      setShowAnimation(true);
      // Show buttons after the main animation
      const buttonTimer = setTimeout(() => setShowButtons(true), 2000);
      return () => clearTimeout(buttonTimer);
    }
  }, [winner]);

  if (!winner) return null;

  return (
    <div className="fixed inset-0 z-50 overflow-hidden">
      {/* Cinematic background */}
      <div className={cn(
        "absolute inset-0",
        theme.bg.primary
      )}>
        {/* Animated rays */}
        <div className="absolute inset-0">
          {[...Array(12)].map((_, i) => (
            <div
              key={i}
              className="absolute top-1/2 left-1/2 w-1 bg-gradient-to-t from-transparent via-red-400 to-transparent opacity-30 animate-ray"
              style={{
                height: '200vh',
                transformOrigin: 'center bottom',
                transform: `translate(-50%, -50%) rotate(${i * 30}deg)`,
                animationDelay: `${i * 0.1}s`,
              }}
            />
          ))}
        </div>
        
        {/* Particle effects */}
        <div className="absolute inset-0">
          {[...Array(50)].map((_, i) => (
            <div
              key={i}
              className="absolute w-1 h-1 bg-red-400 rounded-full animate-particle opacity-70"
              style={{
                left: `${Math.random() * 100}%`,
                top: `${Math.random() * 100}%`,
                animationDelay: `${Math.random() * 3}s`,
                animationDuration: `${3 + Math.random() * 2}s`,
              }}
            />
          ))}
        </div>
      </div>

      {/* Main content */}
      <div className="relative z-10 flex flex-col items-center justify-center h-full text-center px-4">
        {/* VICTORY text */}
        <div className={`transform transition-all duration-1000 ${showAnimation ? 'scale-100 opacity-100' : 'scale-50 opacity-0'}`}>
          <h1 className={cn(
            "text-8xl md:text-9xl font-black drop-shadow-2xl animate-glow mb-4",
            theme.text.accent
          )}>
            VICTORY
          </h1>
        </div>

        {/* Winner info */}
        <div className={`transform transition-all duration-1000 delay-500 ${showAnimation ? 'translate-y-0 opacity-100' : 'translate-y-10 opacity-0'}`}>
          <div className={cn(
            "text-4xl md:text-5xl font-bold mb-2 drop-shadow-lg",
            theme.text.primary
          )}>
            {winner.nickname}
          </div>
          <div className={cn(
            "text-xl mb-8",
            theme.text.secondary
          )}>
            Score final: <span className={cn("font-bold", theme.text.primary)}>{winner.score}</span>
            <br />
            Points marqués: <span className={cn("font-bold", theme.text.accent)}>{state.options.startingScore - winner.score}</span>
          </div>
        </div>

        {/* Discrete buttons */}
        <div className={`transform transition-all duration-500 delay-1000 ${showButtons ? 'translate-y-0 opacity-100' : 'translate-y-5 opacity-0'}`}>
          <div className="flex flex-col sm:flex-row gap-4 mt-8">
            <button
              onClick={() => window.location.reload()}
              className={cn(
                "px-6 py-2 rounded backdrop-blur-sm text-sm font-medium transition-all duration-300",
                theme.button.secondary
              )}
            >
              Nouvelle partie
            </button>
            <button
              onClick={() => {
                alert("Dans une partie normale, les autres joueurs pourraient continuer à jouer leurs tours.");
              }}
              className={cn(
                "px-6 py-2 rounded backdrop-blur-sm text-sm font-medium transition-all duration-300",
                theme.button.primary
              )}
            >
              Continuer à jouer
            </button>
          </div>
        </div>
      </div>

      <style jsx>{`
        @keyframes glow {
          0%, 100% {
            text-shadow: 0 0 20px rgba(255, 215, 0, 0.8), 0 0 40px rgba(255, 215, 0, 0.6), 0 0 60px rgba(255, 215, 0, 0.4);
          }
          50% {
            text-shadow: 0 0 30px rgba(255, 215, 0, 1), 0 0 60px rgba(255, 215, 0, 0.8), 0 0 90px rgba(255, 215, 0, 0.6);
          }
        }
        
        @keyframes ray {
          0% {
            opacity: 0;
            transform: translate(-50%, -50%) rotate(var(--rotation)) scaleY(0);
          }
          50% {
            opacity: 0.3;
            transform: translate(-50%, -50%) rotate(var(--rotation)) scaleY(1);
          }
          100% {
            opacity: 0;
            transform: translate(-50%, -50%) rotate(var(--rotation)) scaleY(0);
          }
        }
        
        @keyframes particle {
          0% {
            transform: translateY(0) scale(0);
            opacity: 0;
          }
          10% {
            opacity: 1;
            transform: translateY(-10px) scale(1);
          }
          90% {
            opacity: 1;
            transform: translateY(-100px) scale(1);
          }
          100% {
            opacity: 0;
            transform: translateY(-120px) scale(0);
          }
        }
        
        .animate-glow {
          animation: glow 2s ease-in-out infinite;
        }
        
        .animate-ray {
          animation: ray 3s ease-in-out infinite;
        }
        
        .animate-particle {
          animation: particle 4s ease-in-out infinite;
        }
      `}</style>
    </div>
  );
}