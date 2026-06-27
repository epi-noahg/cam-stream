/**
 * Hook pour la persistance automatique de l'état de jeu
 */

import { useEffect, useCallback, useRef } from 'react';
import { trpc } from '@/lib/trpc';
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
  
  const updateStateMutation = trpc.game.updateState.useMutation();
  const finishGameMutation = trpc.game.finish.useMutation();
  const lastSavedStateRef = useRef<string>('');

  // Sauvegarde automatique de l'état du jeu
  const saveGameState = useCallback(async () => {
    if (!gameId || !isGameActive || updateStateMutation.isPending) return;

    try {
      // Vérifier si l'état a changé depuis la dernière sauvegarde
      const currentStateString = JSON.stringify(gameState);
      if (currentStateString === lastSavedStateRef.current) return;

      // Sauvegarder en base de données
      await updateStateMutation.mutateAsync({
        id: gameId,
        currentState: currentStateString,
        status: gameState.gameOver ? 'FINISHED' : 'IN_PROGRESS'
      });

      lastSavedStateRef.current = currentStateString;
      console.log('État du jeu sauvegardé:', gameId);
    } catch (error) {
      console.error('Erreur lors de la sauvegarde de l\'état du jeu:', error);
    }
  }, [gameId, gameState, isGameActive, updateStateMutation]);

  // Sauvegarde automatique toutes les 10 secondes
  useEffect(() => {
    if (!isGameActive || !gameId) return;

    const interval = setInterval(saveGameState, 10000);
    return () => clearInterval(interval);
  }, [saveGameState, isGameActive, gameId]);

  // Sauvegarde lors de changements importants
  useEffect(() => {
    if (!isGameActive || !gameId) return;

    const timeoutId = setTimeout(saveGameState, 1000); // Débounce de 1 seconde
    return () => clearTimeout(timeoutId);
  }, [gameState.currentIndex, gameState.turns.length, saveGameState, isGameActive, gameId]);

  // Sauvegarde avant fermeture de la page
  useEffect(() => {
    const handleBeforeUnload = () => {
      if (gameId && isGameActive) {
        navigator.sendBeacon('/api/game-state', JSON.stringify({
          gameId,
          state: gameState
        }));
      }
    };

    window.addEventListener('beforeunload', handleBeforeUnload);
    return () => window.removeEventListener('beforeunload', handleBeforeUnload);
  }, [gameId, gameState, isGameActive]);

  // Fonctions utilitaires
  const pauseCurrentGame = useCallback(async () => {
    if (!gameId) return;

    try {
      await updateStateMutation.mutateAsync({
        id: gameId,
        currentState: JSON.stringify(gameState),
        status: 'PAUSED'
      });
    } catch (error) {
      console.error('Erreur lors de la mise en pause:', error);
      throw error;
    }
  }, [gameId, gameState, updateStateMutation]);

  const finishCurrentGame = useCallback(async (winnerId: number, finalRank: number[]) => {
    if (!gameId) return;

    try {
      const finalState = { ...gameState, winnerId, finalRank };
      
      await finishGameMutation.mutateAsync({
        id: gameId,
        winnerId,
        winnerRank: finalRank,
        finalState: JSON.stringify(finalState)
      });
    } catch (error) {
      console.error('Erreur lors de la finalisation:', error);
      throw error;
    }
  }, [gameId, gameState, finishGameMutation]);

  return {
    saveGameState,
    pauseCurrentGame,
    finishCurrentGame,
    isSaving: updateStateMutation.isPending,
    isPausing: updateStateMutation.isPending,
    isFinishing: finishGameMutation.isPending,
    error: updateStateMutation.error || finishGameMutation.error
  };
}