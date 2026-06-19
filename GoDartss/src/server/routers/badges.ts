/**
 * Routeur tRPC pour la gestion des badges et achievements
 */

import { z } from 'zod';
import { router, procedure } from '../trpc';
import { BadgeType } from '@prisma/client';

export const badgesRouter = router({
  // Attribuer un badge à un joueur
  award: procedure
    .input(z.object({
      playerId: z.number(),
      type: z.nativeEnum(BadgeType),
      gameId: z.number().optional(),
      metadata: z.record(z.string(), z.unknown()).optional(),
    }))
    .mutation(async ({ ctx, input }) => {
      try {
        const badge = await ctx.prisma.playerBadge.create({
          data: {
            playerId: input.playerId,
            type: input.type,
            gameId: input.gameId,
            metadata: input.metadata as any,
          },
          include: {
            player: true
          }
        });
        return badge;
      } catch (error: unknown) {
        // Le badge existe déjà pour ce joueur (contrainte unique)
        if ((error as any)?.code === 'P2002') {
          return null;
        }
        throw error;
      }
    }),

  // Récupérer tous les badges d'un joueur
  getByPlayerId: procedure
    .input(z.object({ playerId: z.number() }))
    .query(async ({ ctx, input }) => {
      return await ctx.prisma.playerBadge.findMany({
        where: { playerId: input.playerId },
        orderBy: {
          earnedAt: 'desc'
        }
      });
    }),

  // Récupérer tous les badges avec les détails des joueurs
  getAll: procedure
    .input(z.object({
      type: z.nativeEnum(BadgeType).optional(),
      limit: z.number().min(1).max(100).default(50),
    }))
    .query(async ({ ctx, input }) => {
      const where: Record<string, unknown> = {};
      
      if (input.type) {
        where.type = input.type;
      }

      return await ctx.prisma.playerBadge.findMany({
        where,
        include: {
          player: true
        },
        orderBy: {
          earnedAt: 'desc'
        },
        take: input.limit,
      });
    }),

  // Vérifier et attribuer automatiquement les badges pour un tour
  checkAndAwardForTurn: procedure
    .input(z.object({
      playerId: z.number(),
      gameId: z.number(),
      turnData: z.object({
        throws: z.array(z.object({
          value: z.number(),
          multiplier: z.number(),
        })),
        totalScore: z.number(),
      }),
      gameState: z.record(z.string(), z.any()).optional(), // État actuel du jeu pour contextualiser
    }))
    .mutation(async ({ ctx, input }) => {
      const awardedBadges: Array<Record<string, unknown>> = [];
      const { throws, totalScore } = input.turnData;

      try {
        // Badge: 3 triple 20 dans le même tour
        const triple20Count = throws.filter(t => t.value === 20 && t.multiplier === 3).length;
        if (triple20Count === 3) {
          const badge = await ctx.prisma.playerBadge.upsert({
            where: {
              playerId_type: {
                playerId: input.playerId,
                type: BadgeType.TRIPLE_TWENTY_HAT_TRICK
              }
            },
            update: {},
            create: {
              playerId: input.playerId,
              type: BadgeType.TRIPLE_TWENTY_HAT_TRICK,
              gameId: input.gameId,
              metadata: { turnScore: totalScore, throws } as any
            }
          });
          awardedBadges.push(badge);
        }

        // Badge: 3 bulls dans le même tour
        const bullCount = throws.filter(t => t.value === 25 || t.value === 50).length;
        if (bullCount === 3) {
          const badge = await ctx.prisma.playerBadge.upsert({
            where: {
              playerId_type: {
                playerId: input.playerId,
                type: BadgeType.BULL_MASTER
              }
            },
            update: {},
            create: {
              playerId: input.playerId,
              type: BadgeType.BULL_MASTER,
              gameId: input.gameId,
              metadata: { turnScore: totalScore, throws } as any
            }
          });
          awardedBadges.push(badge);
        }

      } catch (error) {
        console.error('Erreur lors de l\'attribution des badges:', error);
      }

      return awardedBadges;
    }),

  // Vérifier et attribuer les badges en fin de partie
  checkAndAwardForGameEnd: procedure
    .input(z.object({
      playerId: z.number(),
      gameId: z.number(),
      gameResult: z.object({
        isWinner: z.boolean(),
        finalThrow: z.object({
          value: z.number(),
          multiplier: z.number(),
        }).optional(),
        finishScore: z.number().optional(),
        wasComebackWin: z.boolean().optional(),
        dartCount: z.number(), // Nombre total de fléchettes pour finir
      }),
    }))
    .mutation(async ({ ctx, input }) => {
      const awardedBadges: Array<Record<string, unknown>> = [];
      const { gameResult } = input;

      if (!gameResult.isWinner) {
        return awardedBadges;
      }

      try {
        // Badge: Double Out Finish
        if (gameResult.finalThrow && gameResult.finalThrow.multiplier === 2) {
          const badge = await ctx.prisma.playerBadge.upsert({
            where: {
              playerId_type: {
                playerId: input.playerId,
                type: BadgeType.DOUBLE_OUT_FINISH
              }
            },
            update: {},
            create: {
              playerId: input.playerId,
              type: BadgeType.DOUBLE_OUT_FINISH,
              gameId: input.gameId,
              metadata: { 
                finalThrow: gameResult.finalThrow,
                finishScore: gameResult.finishScore 
              } as any
            }
          });
          awardedBadges.push(badge);
        }

        // Badge: High Finish (plus de 100 points)
        if (gameResult.finishScore && gameResult.finishScore > 100) {
          const badge = await ctx.prisma.playerBadge.upsert({
            where: {
              playerId_type: {
                playerId: input.playerId,
                type: BadgeType.HIGH_FINISH
              }
            },
            update: {},
            create: {
              playerId: input.playerId,
              type: BadgeType.HIGH_FINISH,
              gameId: input.gameId,
              metadata: { 
                finishScore: gameResult.finishScore,
                finalThrow: gameResult.finalThrow 
              }
            }
          });
          awardedBadges.push(badge);
        }

        // Badge: Perfect Leg (9 fléchettes)
        if (gameResult.dartCount === 9) {
          const badge = await ctx.prisma.playerBadge.upsert({
            where: {
              playerId_type: {
                playerId: input.playerId,
                type: BadgeType.PERFECT_LEG
              }
            },
            update: {},
            create: {
              playerId: input.playerId,
              type: BadgeType.PERFECT_LEG,
              gameId: input.gameId,
              metadata: { dartCount: gameResult.dartCount } as any
            }
          });
          awardedBadges.push(badge);
        }

        // Badge: Comeback King
        if (gameResult.wasComebackWin) {
          const badge = await ctx.prisma.playerBadge.upsert({
            where: {
              playerId_type: {
                playerId: input.playerId,
                type: BadgeType.COMEBACK_KING
              }
            },
            update: {},
            create: {
              playerId: input.playerId,
              type: BadgeType.COMEBACK_KING,
              gameId: input.gameId,
              metadata: { gameId: input.gameId } as any
            }
          });
          awardedBadges.push(badge);
        }

      } catch (error) {
        console.error('Erreur lors de l\'attribution des badges de fin de partie:', error);
      }

      return awardedBadges;
    }),

  // Supprimer un badge (en cas d'erreur)
  revoke: procedure
    .input(z.object({
      playerId: z.number(),
      type: z.nativeEnum(BadgeType),
    }))
    .mutation(async ({ ctx, input }) => {
      return await ctx.prisma.playerBadge.delete({
        where: {
          playerId_type: {
            playerId: input.playerId,
            type: input.type
          }
        }
      });
    }),

  // Récupérer les statistiques des badges
  getStatistics: procedure
    .query(async ({ ctx }) => {
      const badgeStats = await ctx.prisma.playerBadge.groupBy({
        by: ['type'],
        _count: {
          type: true
        }
      });

      return badgeStats.map(stat => ({
        type: stat.type,
        count: stat._count.type
      }));
    }),
});