/**
 * Hook pour reprendre une partie sauvegardée
 */

import { useState, useEffect } from 'react';
// import { api } from '@/app/providers'; // Désactivé temporairement
import { GameState } from '@/types/game';

export function useGameResume() {
  const [resumableGames, setResumableGames] = useState<any[]>([]);
  const [isLoading, setIsLoading] = useState(true);

  // Version simplifiée - désactivée temporairement
  const inProgressGames: any[] = [];
  const pausedGames: any[] = [];
  const resumeGame = { 
    mutateAsync: async (_: any) => {},
    isPending: false,
    error: null
  };

  useEffect(() => {
    const games = [...(inProgressGames || []), ...(pausedGames || [])];
    setResumableGames(games);
    setIsLoading(false);
  }, [inProgressGames, pausedGames]);

  const restoreGameState = (game: any): GameState | null => {
    if (!game.currentState) return null;

    try {
      const state = JSON.parse(game.currentState) as GameState;
      return state;
    } catch (error) {
      console.error('Erreur lors de la restauration de l\'état du jeu:', error);
      return null;
    }
  };

  const resumeGameById = async (gameId: number) => {
    try {
      const game = await resumeGame.mutateAsync({ id: gameId });
      return game;
    } catch (error) {
      console.error('Erreur lors de la reprise de la partie:', error);
      throw error;
    }
  };

  const getLastPlayedGame = (playerId?: number) => {
    if (!playerId || resumableGames.length === 0) return null;

    return resumableGames
      .filter(game => 
        game.participants.some((p: any) => p.playerId === playerId)
      )
      .sort((a, b) => new Date(b.updatedAt).getTime() - new Date(a.updatedAt).getTime())[0];
  };

  return {
    resumableGames,
    isLoading,
    restoreGameState,
    resumeGameById,
    getLastPlayedGame,
    isResuming: resumeGame.isPending,
    resumeError: resumeGame.error
  };
}