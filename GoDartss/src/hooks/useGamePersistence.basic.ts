/**
 * Hook simplifié pour la persistance - version sans tRPC pour corriger les erreurs
 */

import { useEffect, useCallback } from 'react';
import { GameState } from '@/types/game';

interface UseGamePersistenceProps {
  gameId?: number;
  gameState: GameState;
  isGameActive: boolean;
}

export function useGamePersistence({
  gameId,
  gameState,
  isGameActive
}: UseGamePersistenceProps) {
  // Sauvegarde automatique de l'état du jeu (version simplifiée)
  const saveGameState = useCallback(async () => {
    if (!gameId || !isGameActive) return;

    try {
      // Sauvegarde en localStorage pour l'instant
      localStorage.setItem(`gameState_${gameId}`, JSON.stringify(gameState));
    } catch (error) {
      console.error('Erreur lors de la sauvegarde de l\'état du jeu:', error);
    }
  }, [gameId, gameState, isGameActive]);

  // Sauvegarde automatique toutes les 30 secondes
  useEffect(() => {
    if (!isGameActive || !gameId) return;

    const interval = setInterval(saveGameState, 30000);
    return () => clearInterval(interval);
  }, [saveGameState, isGameActive, gameId]);

  // Sauvegarde lors de changements importants
  useEffect(() => {
    if (!isGameActive || !gameId) return;

    const timeoutId = setTimeout(saveGameState, 1000);
    return () => clearTimeout(timeoutId);
  }, [gameState.currentIndex, gameState.turns.length, saveGameState, isGameActive, gameId]);

  // Fonctions utilitaires
  const pauseCurrentGame = useCallback(async () => {
    if (!gameId) return;
    // Implémentation simplifiée
    localStorage.setItem(`gamePaused_${gameId}`, 'true');
  }, [gameId]);

  const finishCurrentGame = useCallback(async (winnerId: number, finalRank: number[]) => {
    if (!gameId) return;
    // Implémentation simplifiée
    localStorage.setItem(`gameFinished_${gameId}`, JSON.stringify({ winnerId, finalRank }));
  }, [gameId]);

  return {
    saveGameState,
    pauseCurrentGame,
    finishCurrentGame,
    isSaving: false,
    isPausing: false,
    isFinishing: false,
    error: null
  };
}