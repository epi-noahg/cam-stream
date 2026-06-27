/**
 * Système de badges et achievements pour GoDarts
 */

import { BadgeType } from '@prisma/client';
import { Throw, PlayerState, GameState } from '@/types/game';

export interface BadgeDefinition {
  type: BadgeType;
  name: string;
  description: string;
  icon: string;
  rarity: 'common' | 'rare' | 'epic' | 'legendary';
  checkCondition: (context: BadgeCheckContext) => boolean;
}

export interface BadgeCheckContext {
  currentTurn: Throw[];
  playerState: PlayerState;
  gameState: GameState;
  turnHistory: Throw[][];
  isGameEnd?: boolean;
  isWinner?: boolean;
  finalThrow?: Throw;
}

export const BADGE_DEFINITIONS: BadgeDefinition[] = [
  {
    type: BadgeType.TRIPLE_TWENTY_HAT_TRICK,
    name: "Triple 20 Hat Trick",
    description: "Réalisez 3 triple 20 dans le même tour",
    icon: "🎯",
    rarity: 'rare',
    checkCondition: ({ currentTurn }) => {
      const triple20Count = currentTurn.filter(
        t => t.value === 20 && t.multiplier === 3
      ).length;
      return triple20Count === 3;
    }
  },
  {
    type: BadgeType.BULL_MASTER,
    name: "Bull Master",
    description: "Réalisez 3 bulls dans le même tour",
    icon: "🎯",
    rarity: 'epic',
    checkCondition: ({ currentTurn }) => {
      const bullCount = currentTurn.filter(
        t => t.value === 25 || t.value === 50
      ).length;
      return bullCount === 3;
    }
  },
  {
    type: BadgeType.DOUBLE_OUT_FINISH,
    name: "Double Out Master",
    description: "Terminez une partie avec un double",
    icon: "🏆",
    rarity: 'common',
    checkCondition: ({ isGameEnd, isWinner, finalThrow }) => {
      return !!(isGameEnd && isWinner && finalThrow?.multiplier === 2);
    }
  },
  {
    type: BadgeType.HIGH_FINISH,
    name: "High Finisher",
    description: "Terminez une partie avec plus de 100 points",
    icon: "💯",
    rarity: 'rare',
    checkCondition: ({ isGameEnd, isWinner, playerState, currentTurn }) => {
      if (!isGameEnd || !isWinner) return false;
      
      const turnScore = currentTurn.reduce(
        (sum, t) => sum + (t.value * t.multiplier), 0
      );
      
      return turnScore > 100 && playerState.score === 0;
    }
  },
  {
    type: BadgeType.PERFECT_LEG,
    name: "Perfect Leg",
    description: "Terminez une partie en exactement 9 fléchettes",
    icon: "✨",
    rarity: 'legendary',
    checkCondition: ({ isGameEnd, isWinner, turnHistory, currentTurn }) => {
      if (!isGameEnd || !isWinner) return false;
      
      const totalDarts = turnHistory.reduce((sum, turn) => sum + turn.length, 0) + currentTurn.length;
      return totalDarts === 9;
    }
  },
  {
    type: BadgeType.CONSISTENT_SCORER,
    name: "Consistent Scorer",
    description: "Réalisez 5 tours consécutifs de plus de 40 points",
    icon: "📈",
    rarity: 'common',
    checkCondition: ({ turnHistory, currentTurn }) => {
      const allTurns = [...turnHistory, currentTurn];
      
      if (allTurns.length < 5) return false;
      
      const lastFiveTurns = allTurns.slice(-5);
      return lastFiveTurns.every(turn => {
        const score = turn.reduce((sum, t) => sum + (t.value * t.multiplier), 0);
        return score > 40;
      });
    }
  },
  {
    type: BadgeType.COMEBACK_KING,
    name: "Comeback King",
    description: "Gagnez après avoir été en dernière position",
    icon: "👑",
    rarity: 'epic',
    checkCondition: ({ isGameEnd, isWinner, gameState }) => {
      if (!isGameEnd || !isWinner) return false;
      
      // Vérifier si le joueur était en dernière position à un moment donné
      // Cette logique nécessiterait un tracking plus avancé de l'historique des positions
      return false; // Implémentation simplifiée
    }
  }
];

export class BadgeChecker {
  /**
   * Vérifie quels badges peuvent être attribués pour un tour donné
   */
  static checkForBadges(context: BadgeCheckContext): BadgeType[] {
    const earnedBadges: BadgeType[] = [];
    
    for (const badge of BADGE_DEFINITIONS) {
      try {
        if (badge.checkCondition(context)) {
          earnedBadges.push(badge.type);
        }
      } catch (error) {
        console.error(`Erreur lors de la vérification du badge ${badge.type}:`, error);
      }
    }
    
    return earnedBadges;
  }

  /**
   * Obtient la définition d'un badge par son type
   */
  static getBadgeDefinition(type: BadgeType): BadgeDefinition | undefined {
    return BADGE_DEFINITIONS.find(badge => badge.type === type);
  }

  /**
   * Vérifie les badges en fin de partie
   */
  static checkGameEndBadges(
    playerState: PlayerState,
    gameState: GameState,
    finalTurn: Throw[]
  ): BadgeType[] {
    const turnHistory = playerState.throws
      .reduce((acc: Throw[][], throwData, index) => {
        const turnIndex = Math.floor(index / 3);
        if (!acc[turnIndex]) acc[turnIndex] = [];
        acc[turnIndex].push(throwData);
        return acc;
      }, []);

    const context: BadgeCheckContext = {
      currentTurn: finalTurn,
      playerState,
      gameState,
      turnHistory,
      isGameEnd: true,
      isWinner: gameState.winner === playerState.id,
      finalThrow: finalTurn[finalTurn.length - 1]
    };

    return this.checkForBadges(context);
  }

  /**
   * Vérifie les badges pour un tour normal
   */
  static checkTurnBadges(
    currentTurn: Throw[],
    playerState: PlayerState,
    gameState: GameState
  ): BadgeType[] {
    const turnHistory = playerState.throws
      .reduce((acc: Throw[][], throwData, index) => {
        const turnIndex = Math.floor(index / 3);
        if (!acc[turnIndex]) acc[turnIndex] = [];
        acc[turnIndex].push(throwData);
        return acc;
      }, []);

    const context: BadgeCheckContext = {
      currentTurn,
      playerState,
      gameState,
      turnHistory,
      isGameEnd: false,
      isWinner: false
    };

    return this.checkForBadges(context);
  }
}

/**
 * Utilitaires pour l'affichage des badges
 */
export const badgeUtils = {
  getRarityColor: (rarity: BadgeDefinition['rarity']) => {
    switch (rarity) {
      case 'common': return 'text-gray-600';
      case 'rare': return 'text-blue-600';
      case 'epic': return 'text-purple-600';
      case 'legendary': return 'text-yellow-600';
      default: return 'text-gray-600';
    }
  },

  getRarityBorder: (rarity: BadgeDefinition['rarity']) => {
    switch (rarity) {
      case 'common': return 'border-gray-300';
      case 'rare': return 'border-blue-300';
      case 'epic': return 'border-purple-300';
      case 'legendary': return 'border-yellow-300';
      default: return 'border-gray-300';
    }
  },

  formatBadgeProgress: (current: number, required: number) => {
    return `${current}/${required}`;
  }
};