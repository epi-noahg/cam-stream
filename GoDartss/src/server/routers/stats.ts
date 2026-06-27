/**
 * Routeur tRPC pour la gestion des statistiques
 */

import { z } from 'zod';
import { router, procedure } from '../trpc';

export const statsRouter = router({
  // Mettre à jour les statistiques d'un joueur après un lancer
  updatePlayerStats: procedure
    .input(z.object({
      playerId: z.number(),
      throws: z.array(z.object({
        value: z.number(),
        multiplier: z.number(),
      })),
      isCheckoutAttempt: z.boolean().default(false),
      isCheckoutSuccess: z.boolean().default(false),
      checkoutScore: z.number().optional(),
    }))
    .mutation(async ({ ctx, input }) => {
      const { throws } = input;
      
      // Calculer les statistiques du tour
      let totalScore = 0;
      let tripleCount = 0;
      let doubleCount = 0;
      let bull50Count = 0;
      let bull25Count = 0;
      
      throws.forEach(throwData => {
        const score = throwData.value * throwData.multiplier;
        totalScore += score;
        
        if (throwData.multiplier === 3) tripleCount++;
        if (throwData.multiplier === 2) doubleCount++;
        if (throwData.value === 50) bull50Count++;
        if (throwData.value === 25) bull25Count++;
      });

      const throwCount = throws.length;
      const is180 = totalScore === 180;
      const threeDartAverage = throwCount === 3 ? totalScore / 3 : 0;

      // Récupérer ou créer les statistiques du joueur
      const existingStats = await ctx.prisma.playerStats.findUnique({
        where: { playerId: input.playerId }
      });

      if (!existingStats) {
        // Créer de nouvelles statistiques
        return await ctx.prisma.playerStats.create({
          data: {
            playerId: input.playerId,
            totalThrows: throwCount,
            totalScore: totalScore,
            averageScore: totalScore / throwCount,
            tripleCount,
            doubleCount,
            bull50Count,
            bull25Count,
            total180s: is180 ? 1 : 0,
            bestThreeDartAverage: threeDartAverage,
            checkoutAttempts: input.isCheckoutAttempt ? 1 : 0,
            checkoutSuccess: input.isCheckoutSuccess ? 1 : 0,
            averageCheckoutScore: input.checkoutScore || 0,
          }
        });
      } else {
        // Mettre à jour les statistiques existantes
        const newTotalThrows = existingStats.totalThrows + throwCount;
        const newTotalScore = existingStats.totalScore + totalScore;
        const newAverageScore = newTotalScore / newTotalThrows;
        
        const newTotal180s = existingStats.total180s + (is180 ? 1 : 0);
        const newBestAverage = Math.max(existingStats.bestThreeDartAverage, threeDartAverage);
        
        // Calculer la nouvelle moyenne des checkouts
        let newAverageCheckout = existingStats.averageCheckoutScore;
        if (input.isCheckoutSuccess && input.checkoutScore) {
          const totalCheckoutScore = (existingStats.averageCheckoutScore * existingStats.checkoutSuccess) + input.checkoutScore;
          const totalCheckouts = existingStats.checkoutSuccess + 1;
          newAverageCheckout = totalCheckoutScore / totalCheckouts;
        }

        return await ctx.prisma.playerStats.update({
          where: { playerId: input.playerId },
          data: {
            totalThrows: newTotalThrows,
            totalScore: newTotalScore,
            averageScore: newAverageScore,
            tripleCount: existingStats.tripleCount + tripleCount,
            doubleCount: existingStats.doubleCount + doubleCount,
            bull50Count: existingStats.bull50Count + bull50Count,
            bull25Count: existingStats.bull25Count + bull25Count,
            total180s: newTotal180s,
            bestThreeDartAverage: newBestAverage,
            checkoutAttempts: existingStats.checkoutAttempts + (input.isCheckoutAttempt ? 1 : 0),
            checkoutSuccess: existingStats.checkoutSuccess + (input.isCheckoutSuccess ? 1 : 0),
            averageCheckoutScore: newAverageCheckout,
          }
        });
      }
    }),

  // Récupérer les statistiques d'un joueur
  getPlayerStats: procedure
    .input(z.object({ playerId: z.number() }))
    .query(async ({ ctx, input }) => {
      const stats = await ctx.prisma.playerStats.findUnique({
        where: { playerId: input.playerId },
        include: {
          player: true
        }
      });

      if (!stats) {
        return null;
      }

      // Calculer des statistiques dérivées
      const checkoutPercentage = stats.checkoutAttempts > 0 
        ? (stats.checkoutSuccess / stats.checkoutAttempts) * 100 
        : 0;
        
      const triplePercentage = stats.totalThrows > 0 
        ? (stats.tripleCount / stats.totalThrows) * 100 
        : 0;
        
      const doublePercentage = stats.totalThrows > 0 
        ? (stats.doubleCount / stats.totalThrows) * 100 
        : 0;

      return {
        ...stats,
        checkoutPercentage,
        triplePercentage,
        doublePercentage,
      };
    }),

  // Récupérer les statistiques globales de tous les joueurs
  getGlobalStats: procedure
    .query(async ({ ctx }) => {
      const stats = await ctx.prisma.playerStats.findMany({
        include: {
          player: {
            include: {
              _count: {
                select: {
                  wonGames: true,
                  participants: true,
                }
              }
            }
          }
        }
      });

      return stats.map(stat => ({
        ...stat,
        winRate: stat.player._count.participants > 0 
          ? (stat.player._count.wonGames / stat.player._count.participants) * 100 
          : 0,
        checkoutPercentage: stat.checkoutAttempts > 0 
          ? (stat.checkoutSuccess / stat.checkoutAttempts) * 100 
          : 0,
      }));
    }),

  // Récupérer les statistiques de comparaison entre joueurs
  comparePlayerStats: procedure
    .input(z.object({
      playerIds: z.array(z.number()).min(2).max(8),
    }))
    .query(async ({ ctx, input }) => {
      const stats = await ctx.prisma.playerStats.findMany({
        where: {
          playerId: {
            in: input.playerIds
          }
        },
        include: {
          player: {
            include: {
              _count: {
                select: {
                  wonGames: true,
                  participants: true,
                }
              }
            }
          }
        }
      });

      return stats.map(stat => ({
        ...stat,
        winRate: stat.player._count.participants > 0 
          ? (stat.player._count.wonGames / stat.player._count.participants) * 100 
          : 0,
        checkoutPercentage: stat.checkoutAttempts > 0 
          ? (stat.checkoutSuccess / stat.checkoutAttempts) * 100 
          : 0,
      }));
    }),

  // Récupérer les records globaux
  getRecords: procedure
    .query(async ({ ctx }) => {
      const stats = await ctx.prisma.playerStats.findMany({
        include: {
          player: true
        }
      });

      if (stats.length === 0) {
        return null;
      }

      const bestAverage = Math.max(...stats.map(s => s.averageScore));
      const most180s = Math.max(...stats.map(s => s.total180s));
      const bestCheckoutAverage = Math.max(...stats.map(s => s.averageCheckoutScore));
      const bestThreeDartAverage = Math.max(...stats.map(s => s.bestThreeDartAverage));

      return {
        bestAverageScore: {
          value: bestAverage,
          player: stats.find(s => s.averageScore === bestAverage)?.player
        },
        most180s: {
          value: most180s,
          player: stats.find(s => s.total180s === most180s)?.player
        },
        bestCheckoutAverage: {
          value: bestCheckoutAverage,
          player: stats.find(s => s.averageCheckoutScore === bestCheckoutAverage)?.player
        },
        bestThreeDartAverage: {
          value: bestThreeDartAverage,
          player: stats.find(s => s.bestThreeDartAverage === bestThreeDartAverage)?.player
        }
      };
    }),

  // Réinitialiser les statistiques d'un joueur
  resetPlayerStats: procedure
    .input(z.object({ playerId: z.number() }))
    .mutation(async ({ ctx, input }) => {
      return await ctx.prisma.playerStats.update({
        where: { playerId: input.playerId },
        data: {
          averageScore: 0,
          totalThrows: 0,
          totalScore: 0,
          bull50Count: 0,
          bull25Count: 0,
          tripleCount: 0,
          doubleCount: 0,
          checkoutAttempts: 0,
          checkoutSuccess: 0,
          averageCheckoutScore: 0,
          bestThreeDartAverage: 0,
          longest180Series: 0,
          total180s: 0,
        }
      });
    }),
});