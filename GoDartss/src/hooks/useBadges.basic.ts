/**
 * Hook simplifié pour les badges - version sans tRPC pour corriger les erreurs
 */

import { useState, useCallback } from 'react';
import { BadgeDefinition, BADGE_DEFINITIONS } from '@/lib/badgeSystem';
import { BadgeType } from '@prisma/client';
import { Throw, PlayerState, GameState } from '@/types/game';

export function useBadges() {
  const [recentlyEarnedBadges, setRecentlyEarnedBadges] = useState<BadgeDefinition[]>([]);

  // Vérifier et attribuer les badges pour un tour (version simplifiée)
  const checkTurnBadges = useCallback(async (
    playerId: number,
    gameId: number,
    currentTurn: Throw[],
    playerState: PlayerState,
    gameState: GameState
  ) => {
    try {
      // Logique simplifiée - juste vérifier localement
      const earnedBadges: BadgeType[] = [];
      
      // Triple 20 Hat Trick
      const triple20Count = currentTurn.filter(t => t.value === 20 && t.multiplier === 3).length;
      if (triple20Count === 3) {
        earnedBadges.push(BadgeType.TRIPLE_TWENTY_HAT_TRICK);
      }

      // Bull Master
      const bullCount = currentTurn.filter(t => t.value === 25 || t.value === 50).length;
      if (bullCount === 3) {
        earnedBadges.push(BadgeType.BULL_MASTER);
      }

      // Ajouter les badges pour affichage
      const newBadgeDefinitions = earnedBadges
        .map(type => BADGE_DEFINITIONS.find(def => def.type === type))
        .filter((def): def is BadgeDefinition => def !== undefined);

      if (newBadgeDefinitions.length > 0) {
        setRecentlyEarnedBadges(prev => [...prev, ...newBadgeDefinitions]);
        
        setTimeout(() => {
          setRecentlyEarnedBadges(prev => 
            prev.filter(badge => !newBadgeDefinitions.includes(badge))
          );
        }, 5000);
      }

      return earnedBadges;
    } catch (error) {
      console.error('Erreur lors de la vérification des badges:', error);
      return [];
    }
  }, []);

  // Vérifier et attribuer les badges en fin de partie (version simplifiée)
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
      const earnedBadges: BadgeType[] = [];

      if (gameResult.isWinner) {
        // Double Out Finish
        if (gameResult.finalThrow && gameResult.finalThrow.multiplier === 2) {
          earnedBadges.push(BadgeType.DOUBLE_OUT_FINISH);
        }

        // High Finish
        if (gameResult.finishScore && gameResult.finishScore > 100) {
          earnedBadges.push(BadgeType.HIGH_FINISH);
        }

        // Perfect Leg
        if (gameResult.dartCount === 9) {
          earnedBadges.push(BadgeType.PERFECT_LEG);
        }
      }

      return earnedBadges;
    } catch (error) {
      console.error('Erreur lors de la vérification des badges de fin de partie:', error);
      return [];
    }
  }, []);

  // Attribuer manuellement un badge (version simplifiée)
  const awardManualBadge = useCallback(async (
    playerId: number,
    badgeType: BadgeType,
    gameId?: number,
    metadata?: Record<string, any>
  ) => {
    // Version simplifiée - juste afficher
    const badgeDefinition = BADGE_DEFINITIONS.find(def => def.type === badgeType);
    if (badgeDefinition) {
      setRecentlyEarnedBadges(prev => [...prev, badgeDefinition]);
      
      setTimeout(() => {
        setRecentlyEarnedBadges(prev => 
          prev.filter(b => b.type !== badgeType)
        );
      }, 5000);
    }
    return null;
  }, []);

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
    isCheckingTurn: false,
    isCheckingGameEnd: false,
    isAwarding: false,
    error: null
  };
}