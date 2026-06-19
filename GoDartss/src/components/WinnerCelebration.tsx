"use client";

import { useGame } from "@/context/GameContext";
import { useState, useEffect, useRef, useCallback, useMemo } from "react";
import { theme, cn } from "@/lib/theme";
import { useRouter } from "next/navigation";
import { trpc } from "@/lib/trpc";

interface Particle {
  id: number;
  x: number;
  y: number;
  vx: number;
  vy: number;
  life: number;
  maxLife: number;
  size: number;
  color: string;
  type: 'firework' | 'confetti' | 'spark' | 'star';
  rotation: number;
  rotationSpeed: number;
}

export default function WinnerCelebration() {
  const { state, dispatch, gameId, persistenceHooks } = useGame();
  const router = useRouter();
  const createGameMutation = trpc.game.create.useMutation();
  const [showCelebration, setShowCelebration] = useState(false);
  const [showContent, setShowContent] = useState(false);
  const [particles, setParticles] = useState<Particle[]>([]);
  const animationRef = useRef<number | undefined>(undefined);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  
  // Find the winner
  const winner = state.winner !== null
    ? state.players.find(p => p.id === state.winner)
    : null;

  // Colors for different particle types
  const colors = useMemo(() => [
    '#dc2626', '#991b1b', '#7f1d1d', '#ffffff', '#f3f4f6',
    '#dc2626', '#991b1b', '#7f1d1d', '#ffffff', '#f3f4f6',
    '#dc2626', '#991b1b', '#7f1d1d', '#ffffff', '#f3f4f6'
  ], []);

  // Create explosion of particles
  const createExplosion = useCallback((x: number, y: number, count: number = 15) => {
    const newParticles: Particle[] = [];
    
    for (let i = 0; i < count; i++) {
      const angle = (Math.PI * 2 * i) / count + Math.random() * 0.3;
      const velocity = 0.5 + Math.random() * 1.5; // Further reduced velocity for smoother motion
      const life = 200 + Math.random() * 300; // Longer lifespan for more graceful fade
      
      newParticles.push({
        id: Date.now() + i,
        x,
        y,
        vx: Math.cos(angle) * velocity,
        vy: Math.sin(angle) * velocity,
        life,
        maxLife: life,
        size: 3 + Math.random() * 6, // Increased size for better visibility
        color: colors[Math.floor(Math.random() * colors.length)],
        type: Math.random() > 0.7 ? 'star' : Math.random() > 0.5 ? 'confetti' : 'firework',
        rotation: Math.random() * Math.PI * 2,
        rotationSpeed: (Math.random() - 0.5) * 0.15 // Slower rotation
      });
    }
    
    return newParticles;
  }, [colors]);

  // Create continuous sparkles
  const createSparkles = useCallback(() => {
    const newParticles: Particle[] = [];
    
    for (let i = 0; i < 8; i++) { // Reduced count for better performance
      newParticles.push({
        id: Date.now() + i + 1000,
        x: Math.random() * window.innerWidth,
        y: Math.random() * window.innerHeight,
        vx: (Math.random() - 0.5) * 0.4, // Even slower movement
        vy: (Math.random() - 0.5) * 0.4, // Even slower movement
        life: 240 + Math.random() * 180, // Longer lifespan
        maxLife: 240 + Math.random() * 180,
        size: 2 + Math.random() * 3, // Increased size for better visibility
        color: colors[Math.floor(Math.random() * colors.length)],
        type: 'spark',
        rotation: Math.random() * Math.PI * 2,
        rotationSpeed: (Math.random() - 0.5) * 0.1 // Much slower rotation
      });
    }
    
    return newParticles;
  }, [colors]);

  // Update particles animation
  const updateParticles = useCallback(() => {
    setParticles(prevParticles => {
      return prevParticles
        .map(particle => ({
          ...particle,
          x: particle.x + particle.vx,
          y: particle.y + particle.vy,
          vy: (particle.vy + 0.015) * 0.998, // Much gentler gravity and air resistance
          life: particle.life - 0.8, // Slower life decay for longer visibility
          rotation: particle.rotation + particle.rotationSpeed,
          vx: particle.vx * 0.998 // Less air resistance for smoother motion
        }))
        .filter(particle => particle.life > 0);
    });
  }, []);

  // Canvas animation loop
  const animate = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Clear canvas
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Draw particles
    particles.forEach(particle => {
      const alpha = Math.max(0.3, particle.life / particle.maxLife); // Minimum opacity for better visibility
      ctx.save();
      ctx.globalAlpha = alpha;
      ctx.translate(particle.x, particle.y);
      ctx.rotate(particle.rotation);

      switch (particle.type) {
        case 'firework':
          ctx.fillStyle = particle.color;
          ctx.beginPath();
          ctx.arc(0, 0, particle.size, 0, Math.PI * 2);
          ctx.fill();
          // Add glow effect
          ctx.shadowColor = particle.color;
          ctx.shadowBlur = particle.size * 2;
          ctx.fill();
          break;
          
        case 'confetti':
          ctx.fillStyle = particle.color;
          ctx.fillRect(-particle.size/2, -particle.size/2, particle.size, particle.size * 2);
          // Add glow effect
          ctx.shadowColor = particle.color;
          ctx.shadowBlur = particle.size;
          ctx.fillRect(-particle.size/2, -particle.size/2, particle.size, particle.size * 2);
          break;
          
        case 'star':
          ctx.fillStyle = particle.color;
          ctx.beginPath();
          for (let i = 0; i < 5; i++) {
            const angle = (i * Math.PI * 2) / 5;
            const x = Math.cos(angle) * particle.size;
            const y = Math.sin(angle) * particle.size;
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
          }
          ctx.closePath();
          ctx.fill();
          // Add glow effect
          ctx.shadowColor = particle.color;
          ctx.shadowBlur = particle.size * 1.5;
          ctx.fill();
          break;
          
        case 'spark':
          ctx.strokeStyle = particle.color;
          ctx.lineWidth = particle.size;
          ctx.shadowColor = particle.color;
          ctx.shadowBlur = particle.size * 2;
          ctx.beginPath();
          ctx.moveTo(-particle.size, 0);
          ctx.lineTo(particle.size, 0);
          ctx.stroke();
          break;
      }
      
      ctx.restore();
    });

    updateParticles();
    animationRef.current = requestAnimationFrame(animate);
  }, [particles, updateParticles]);

  // Handle victory sequence
  useEffect(() => {
    if (winner) {
      setShowCelebration(true);
      
      // Initial explosions - staggered for better visual flow with optimized particles
      setTimeout(() => {
        setParticles(prev => [...prev, ...createExplosion(window.innerWidth * 0.2, window.innerHeight * 0.3, 12)]);
      }, 300);
      
      setTimeout(() => {
        setParticles(prev => [...prev, ...createExplosion(window.innerWidth * 0.8, window.innerHeight * 0.3, 12)]);
      }, 600);
      
      setTimeout(() => {
        setParticles(prev => [...prev, ...createExplosion(window.innerWidth * 0.5, window.innerHeight * 0.6, 15)]);
      }, 900);

      // Show content
      setTimeout(() => setShowContent(true), 1000);

      // Continuous sparkles - optimized for performance
      const sparkleInterval = setInterval(() => {
        setParticles(prev => [...prev, ...createSparkles()]);
      }, 2000); // Less frequent sparkles

      // Additional explosions - performance optimized
      const explosionInterval = setInterval(() => {
        const x = Math.random() * window.innerWidth;
        const y = Math.random() * window.innerHeight * 0.7;
        setParticles(prev => [...prev, ...createExplosion(x, y, 8)]); // Smaller explosions
      }, 4000); // Less frequent explosions

      return () => {
        clearInterval(sparkleInterval);
        clearInterval(explosionInterval);
      };
    }
  }, [winner, createExplosion, createSparkles]);

  // Canvas animation
  useEffect(() => {
    if (showCelebration) {
      const canvas = canvasRef.current;
      if (canvas) {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
        animationRef.current = requestAnimationFrame(animate);
      }
    }

    return () => {
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }
    };
  }, [showCelebration, particles, animate]);

  // Handle window resize
  useEffect(() => {
    const handleResize = () => {
      const canvas = canvasRef.current;
      if (canvas) {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
      }
    };

    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  if (!winner || !showCelebration) return null;

  return (
    <div className="fixed inset-0 z-50 pointer-events-none">
      {/* Particle canvas */}
      <canvas
        ref={canvasRef}
        className="absolute inset-0 w-full h-full"
        style={{ pointerEvents: 'none' }}
      />
      
      {/* Modal overlay with elegant dark gradient */}
      <div className="absolute inset-0 bg-gradient-to-br from-gray-900/90 via-red-950/40 to-gray-800/95 backdrop-blur-md flex items-center justify-center">
        {/* Main celebration content */}
        <div className={`
          transform transition-all duration-1000 ease-out pointer-events-auto
          ${showContent ? 'scale-100 opacity-100 translate-y-0' : 'scale-50 opacity-0 translate-y-10'}
        `}>
          {/* Victory card */}
          <div className="backdrop-blur-xl rounded-3xl p-8 shadow-2xl max-w-md mx-4 text-center border-2 bg-gradient-to-br from-gray-800/95 via-gray-900/90 to-red-950/30 border-red-600/60 shadow-red-500/30">
            {/* Trophy icon with glow */}
            <div className="text-8xl mb-6 animate-bounce">
              🏆
            </div>
            
            {/* Victory text */}
            <h1 className="text-4xl font-black mb-4 drop-shadow-lg animate-pulse text-red-400">
              VICTOIRE !
            </h1>
            
            {/* Winner info */}
            <div className="backdrop-blur-sm rounded-2xl p-6 mb-6 border bg-gray-700/50 border-gray-600/60">
              <div className="text-2xl font-bold mb-2 text-white">
                🎯 {winner.nickname}
              </div>
              <div className="text-lg text-gray-300">
                Score final: <span className="font-bold text-white">{winner.score}</span>
              </div>
            </div>
            
            {/* Action buttons */}
            <div className="flex flex-col sm:flex-row gap-3 justify-center">
              {/* Bouton Undo stylé pour la modal */}
              <button
                onClick={() => dispatch({ type: "UNDO" })}
                className="px-2 py-2 rounded-xl font-semibold backdrop-blur-sm transition-all duration-300 transform hover:scale-105 shadow-lg bg-yellow-600/90 hover:bg-yellow-500 text-white border border-yellow-500/80 inline-flex items-center gap-2"
                disabled={!state.turns.some(turn => turn.length > 0) && !state.players.some(player => player.throws.length > 0)}
              >
                Annuler le dernier lancer
              </button>
              
              <button
                onClick={async () => {
                  try {
                    // Finaliser la partie actuelle
                    if (gameId && state.winner) {
                      await persistenceHooks.finishCurrentGame(
                        state.winner,
                        state.finishedPlayers
                      );
                    }
                    
                    // Créer une nouvelle partie avec les mêmes paramètres
                    const newGame = await createGameMutation.mutateAsync({
                      mode: 'X01',
                      playerIds: state.players.map(p => p.id),
                      startingScore: state.options.startingScore,
                      doubleOut: state.options.outType === 'DOUBLE',
                      doubleIn: state.options.inType === 'DOUBLE'
                    });
                    
                    // Rediriger vers la nouvelle partie
                    router.push(`/x01/play/${newGame.id}`);
                  } catch (error) {
                    console.error('Erreur lors de la création de la nouvelle partie:', error);
                  }
                }}
                className="px-6 py-3 rounded-xl font-semibold backdrop-blur-sm transition-all duration-300 transform hover:scale-105 shadow-lg bg-green-600/90 hover:bg-green-500 text-white border border-green-500/80"
                disabled={createGameMutation.isPending}
              >
                {createGameMutation.isPending ? '⏳ Création...' : '🎮 Nouvelle partie'}
              </button>
              <button
                onClick={async () => {
                  try {
                    // Finaliser la partie actuelle
                    if (gameId && state.winner) {
                      await persistenceHooks.finishCurrentGame(
                        state.winner,
                        state.finishedPlayers
                      );
                    }
                    
                    // Retourner à l'accueil
                    router.push('/');
                  } catch (error) {
                    console.error('Erreur lors de la finalisation:', error);
                    // Même en cas d'erreur, retourner à l'accueil
                    router.push('/');
                  }
                }}
                className="px-6 py-3 rounded-xl font-semibold backdrop-blur-sm transition-all duration-300 transform hover:scale-105 shadow-lg bg-red-600/90 hover:bg-red-500 text-white border border-red-500/80"
              >
                🏠 Retour au menu
              </button>
            </div>
          </div>
        </div>
      </div>

    </div>
  );
}