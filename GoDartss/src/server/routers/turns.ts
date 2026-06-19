/**
 * Routeur tRPC pour la gestion des tours et lancers
 */

import { z } from 'zod';
import { router, procedure } from '../trpc';

export const turnsRouter = router({
  // Créer un nouveau tour
  create: procedure
    .input(z.object({
      gameId: z.number(),
      participantId: z.number(),
      orderInGame: z.number(),
      throws: z.array(z.object({
        value: z.number().min(0).max(50),
        multiplier: z.number().min(1).max(3),
      })).max(3),
    }))
    .mutation(async ({ ctx, input }) => {
      const { throws, ...turnData } = input;
      
      const turn = await ctx.prisma.turn.create({
        data: {
          ...turnData,
          throws: {
            create: throws
          }
        },
        include: {
          throws: true,
          participant: {
            include: {
              player: true
            }
          }
        }
      });

      return turn;
    }),

  // Récupérer tous les tours d'une partie
  getByGameId: procedure
    .input(z.object({ gameId: z.number() }))
    .query(async ({ ctx, input }) => {
      return await ctx.prisma.turn.findMany({
        where: { gameId: input.gameId },
        include: {
          throws: true,
          participant: {
            include: {
              player: true
            }
          }
        },
        orderBy: {
          orderInGame: 'asc'
        }
      });
    }),

  // Récupérer tous les tours d'un participant
  getByParticipantId: procedure
    .input(z.object({ participantId: z.number() }))
    .query(async ({ ctx, input }) => {
      return await ctx.prisma.turn.findMany({
        where: { participantId: input.participantId },
        include: {
          throws: true
        },
        orderBy: {
          orderInGame: 'asc'
        }
      });
    }),

  // Mettre à jour un tour (modification des lancers)
  update: procedure
    .input(z.object({
      id: z.number(),
      throws: z.array(z.object({
        value: z.number().min(0).max(50),
        multiplier: z.number().min(1).max(3),
      })).max(3),
    }))
    .mutation(async ({ ctx, input }) => {
      // Supprimer les anciens lancers
      await ctx.prisma.throw.deleteMany({
        where: { turnId: input.id }
      });

      // Créer les nouveaux lancers
      const turn = await ctx.prisma.turn.update({
        where: { id: input.id },
        data: {
          throws: {
            create: input.throws
          }
        },
        include: {
          throws: true,
          participant: {
            include: {
              player: true
            }
          }
        }
      });

      return turn;
    }),

  // Supprimer un tour
  delete: procedure
    .input(z.object({ id: z.number() }))
    .mutation(async ({ ctx, input }) => {
      return await ctx.prisma.turn.delete({
        where: { id: input.id }
      });
    }),

  // Ajouter un lancer à un tour existant
  addThrow: procedure
    .input(z.object({
      turnId: z.number(),
      value: z.number().min(0).max(50),
      multiplier: z.number().min(1).max(3),
    }))
    .mutation(async ({ ctx, input }) => {
      const throwData = await ctx.prisma.throw.create({
        data: {
          turnId: input.turnId,
          value: input.value,
          multiplier: input.multiplier,
        }
      });

      return throwData;
    }),

  // Supprimer un lancer spécifique
  deleteThrow: procedure
    .input(z.object({ id: z.number() }))
    .mutation(async ({ ctx, input }) => {
      return await ctx.prisma.throw.delete({
        where: { id: input.id }
      });
    }),

  // Récupérer les statistiques d'un tour
  getStatistics: procedure
    .input(z.object({ 
      gameId: z.number().optional(),
      participantId: z.number().optional(),
      limit: z.number().default(10),
    }))
    .query(async ({ ctx, input }) => {
      const where: Record<string, unknown> = {};
      
      if (input.gameId) {
        where.gameId = input.gameId;
      }
      
      if (input.participantId) {
        where.participantId = input.participantId;
      }

      const turns = await ctx.prisma.turn.findMany({
        where,
        include: {
          throws: true,
          participant: {
            include: {
              player: true
            }
          }
        },
        orderBy: {
          createdAt: 'desc'
        },
        take: input.limit,
      });

      // Calculer les statistiques
      const stats = turns.map(turn => {
        const totalScore = turn.throws.reduce((sum, throwData) => 
          sum + (throwData.value * throwData.multiplier), 0
        );
        
        const has180 = totalScore === 180;
        const hasTriple20 = turn.throws.some(t => t.value === 20 && t.multiplier === 3);
        const tripleCount = turn.throws.filter(t => t.multiplier === 3).length;
        const doubleCount = turn.throws.filter(t => t.multiplier === 2).length;
        const bullCount = turn.throws.filter(t => t.value === 25 || t.value === 50).length;

        return {
          ...turn,
          totalScore,
          has180,
          hasTriple20,
          tripleCount,
          doubleCount,
          bullCount,
          dartCount: turn.throws.length,
          average: turn.throws.length > 0 ? totalScore / turn.throws.length : 0,
        };
      });

      return stats;
    }),
});