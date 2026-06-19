/**
 * Hook pour la gestion des badges en temps réel
 */

import { useState, useCallback } from 'react';
// import { api } from '@/app/providers'; // Désactivé temporairement
import { BadgeChecker, BadgeDefinition, BADGE_DEFINITIONS } from '@/lib/badgeSystem';
import { BadgeType } from '@prisma/client';
import { Throw, PlayerState, GameState } from '@/types/game';

export function useBadges() {
  const [recentlyEarnedBadges, setRecentlyEarnedBadges] = useState<BadgeDefinition[]>([]);
  
  // Version simplifiée - désactivée temporairement
  const awardBadge = { 
    mutateAsync: async (_: unknown) => null,
    isPending: false,
    error: null
  };
  const checkAndAwardForTurn = { 
    mutateAsync: async (_: unknown) => [],
    isPending: false,
    error: null
  };
  const checkAndAwardForGameEnd = { 
    mutateAsync: async (_: unknown) => [],
    isPending: false,
    error: null
  };

  // Vérifier et attribuer les badges pour un tour
  const checkTurnBadges = useCallback(async (
    playerId: number,
    gameId: number,
    currentTurn: Throw[],
    playerState: PlayerState,
    gameState: GameState
  ) => {
    try {
      // Vérification côté client
      const potentialBadges = BadgeChecker.checkTurnBadges(currentTurn, playerState, gameState);
      
      if (potentialBadges.length > 0) {
        // Vérification et attribution côté serveur
        const turnScore = currentTurn.reduce((sum, t) => sum + (t.value * t.multiplier), 0);
        
        const awardedBadges = await checkAndAwardForTurn.mutateAsync({
          playerId,
          gameId,
          turnData: {
            throws: currentTurn,
            totalScore: turnScore
          }
        });

        // Ajouter les badges récemment gagnés pour l'affichage
        const newBadgeDefinitions = awardedBadges
          .map((badge: any) => BADGE_DEFINITIONS.find(def => def.type === badge.type))
          .filter((def: BadgeDefinition | undefined): def is BadgeDefinition => def !== undefined);

        if (newBadgeDefinitions.length > 0) {
          setRecentlyEarnedBadges(prev => [...prev, ...newBadgeDefinitions]);
          
          // Retirer les badges de l'affichage après 5 secondes
          setTimeout(() => {
            setRecentlyEarnedBadges(prev => 
              prev.filter(badge => !newBadgeDefinitions.includes(badge))
            );
          }, 5000);
        }

        return awardedBadges;
      }
      
      return [];
    } catch (error) {
      console.error('Erreur lors de la vérification des badges:', error);
      return [];
    }
  }, [checkAndAwardForTurn]);

  // Vérifier et attribuer les badges en fin de partie
  const checkGameEndBadges = useCallback(async (
    playerId: number,
    gameId: number,
    gameResult: {
      isWinner: boolean;
      finalThrow?: Throw;
      finishScore?: number;
      wasComebackWin?: boolean;
      dartCount: number;
    }
  ) => {
    try {
      const awardedBadges = await checkAndAwardForGameEnd.mutateAsync({
        playerId,
        gameId,
        gameResult
      });

      // Ajouter les badges récemment gagnés pour l'affichage
      const newBadgeDefinitions = awardedBadges
        .map((badge: any) => BADGE_DEFINITIONS.find(def => def.type === badge.type))
        .filter((def: BadgeDefinition | undefined): def is BadgeDefinition => def !== undefined);

      if (newBadgeDefinitions.length > 0) {
        setRecentlyEarnedBadges(prev => [...prev, ...newBadgeDefinitions]);
        
        // Retirer les badges de l'affichage après 8 secondes (plus long pour fin de partie)
        setTimeout(() => {
          setRecentlyEarnedBadges(prev => 
            prev.filter(badge => !newBadgeDefinitions.includes(badge))
          );
        }, 8000);
      }

      return awardedBadges;
    } catch (error) {
      console.error('Erreur lors de la vérification des badges de fin de partie:', error);
      return [];
    }
  }, [checkAndAwardForGameEnd]);

  // Attribuer manuellement un badge (pour les cas spéciaux)
  const awardManualBadge = useCallback(async (
    playerId: number,
    badgeType: BadgeType,
    gameId?: number,
    metadata?: Record<string, any>
  ) => {
    try {
      const badge = await awardBadge.mutateAsync({
        playerId,
        type: badgeType,
        gameId,
        metadata
      });

      if (badge) {
        const badgeDefinition = BADGE_DEFINITIONS.find(def => def.type === badgeType);
        if (badgeDefinition) {
          setRecentlyEarnedBadges(prev => [...prev, badgeDefinition]);
          
          setTimeout(() => {
            setRecentlyEarnedBadges(prev => 
              prev.filter(b => b.type !== badgeType)
            );
          }, 5000);
        }
      }

      return badge;
    } catch (error) {
      console.error('Erreur lors de l\'attribution manuelle du badge:', error);
      throw error;
    }
  }, [awardBadge]);

  // Effacer les badges récemment affichés
  const clearRecentBadges = useCallback(() => {
    setRecentlyEarnedBadges([]);
  }, []);

  return {
    recentlyEarnedBadges,
    checkTurnBadges,
    checkGameEndBadges,
    awardManualBadge,
    clearRecentBadges,
    isCheckingTurn: checkAndAwardForTurn.isPending,
    isCheckingGameEnd: checkAndAwardForGameEnd.isPending,
    isAwarding: awardBadge.isPending,
    error: checkAndAwardForTurn.error || checkAndAwardForGameEnd.error || awardBadge.error
  };
}