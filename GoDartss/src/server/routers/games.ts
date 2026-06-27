/**
 * Routeur tRPC pour la gestion des parties
 */

import { z } from 'zod';
import { router, procedure } from '../trpc';
import { GameMode, GameStatus } from '@prisma/client';

export const gamesRouter = router({
  // Créer une nouvelle partie
  create: procedure
    .input(z.object({
      mode: z.nativeEnum(GameMode),
      playerIds: z.array(z.number()).min(1).max(8),
      startingScore: z.number().optional(),
      doubleOut: z.boolean().default(false),
      doubleIn: z.boolean().default(false),
    }))
    .mutation(async ({ ctx, input }) => {
      const game = await ctx.prisma.game.create({
        data: {
          mode: input.mode,
          startingScore: input.startingScore,
          doubleOut: input.doubleOut,
          doubleIn: input.doubleIn,
          status: GameStatus.WAITING,
          participants: {
            create: input.playerIds.map((playerId, index) => ({
              playerId,
              order: index,
            }))
          }
        },
        include: {
          participants: {
            include: {
              player: true
            },
            orderBy: {
              order: 'asc'
            }
          }
        }
      });

      return game;
    }),

  // Récupérer toutes les parties
  getAll: procedure
    .input(z.object({
      status: z.nativeEnum(GameStatus).optional(),
      playerId: z.number().optional(),
      limit: z.number().min(1).max(100).default(20),
      offset: z.number().default(0),
    }))
    .query(async ({ ctx, input }) => {
      const where: Record<string, unknown> = {};
      
      if (input.status) {
        where.status = input.status;
      }
      
      if (input.playerId) {
        where.participants = {
          some: {
            playerId: input.playerId
          }
        };
      }

      return await ctx.prisma.game.findMany({
        where,
        include: {
          participants: {
            include: {
              player: true
            },
            orderBy: {
              order: 'asc'
            }
          },
          winner: true,
          _count: {
            select: {
              turns: true
            }
          }
        },
        orderBy: {
          createdAt: 'desc'
        },
        take: input.limit,
        skip: input.offset,
      });
    }),

  // Récupérer une partie par ID
  getById: procedure
    .input(z.object({ id: z.number() }))
    .query(async ({ ctx, input }) => {
      return await ctx.prisma.game.findUnique({
        where: { id: input.id },
        include: {
          participants: {
            include: {
              player: true,
              turns: {
                include: {
                  throws: true
                },
                orderBy: {
                  orderInGame: 'asc'
                }
              }
            },
            orderBy: {
              order: 'asc'
            }
          },
          winner: true,
          turns: {
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
          }
        }
      });
    }),

  // Récupérer la partie en cours d'un joueur (s'il y en a une)
  getCurrentGame: procedure
    .input(z.object({ playerId: z.number() }))
    .query(async ({ ctx, input }) => {
      return await ctx.prisma.game.findFirst({
        where: {
          participants: {
            some: {
              playerId: input.playerId
            }
          },
          status: {
            in: [GameStatus.IN_PROGRESS, GameStatus.PAUSED]
          }
        },
        include: {
          participants: {
            include: {
              player: true,
              turns: {
                include: {
                  throws: true
                },
                orderBy: {
                  orderInGame: 'asc'
                }
              }
            },
            orderBy: {
              order: 'asc'
            }
          },
          turns: {
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
          }
        },
        orderBy: {
          updatedAt: 'desc'
        }
      });
    }),

  // Démarrer une partie
  start: procedure
    .input(z.object({ 
      id: z.number(),
      initialState: z.string().optional() // État initial du jeu en JSON
    }))
    .mutation(async ({ ctx, input }) => {
      return await ctx.prisma.game.update({
        where: { id: input.id },
        data: {
          status: GameStatus.IN_PROGRESS,
          currentState: input.initialState,
          updatedAt: new Date(),
        },
        include: {
          participants: {
            include: {
              player: true
            },
            orderBy: {
              order: 'asc'
            }
          }
        }
      });
    }),


  // Terminer une partie
  finish: procedure
    .input(z.object({
      id: z.number(),
      winnerId: z.number(),
      winnerRank: z.array(z.number()), // Classement final des joueurs
      finalState: z.string(),
    }))
    .mutation(async ({ ctx, input }) => {
      // Mettre à jour la partie
      const game = await ctx.prisma.game.update({
        where: { id: input.id },
        data: {
          status: GameStatus.FINISHED,
          winnerId: input.winnerId,
          winnerRank: input.winnerRank,
          currentState: input.finalState,
          finishedAt: new Date(),
          updatedAt: new Date(),
        },
        include: {
          participants: {
            include: {
              player: true
            }
          }
        }
      });

      // Mettre à jour les statistiques des joueurs
      for (const participant of game.participants) {
        const isWinner = participant.playerId === input.winnerId;
        
        await ctx.prisma.player.update({
          where: { id: participant.playerId },
          data: {
            totalGames: { increment: 1 },
            totalWins: isWinner ? { increment: 1 } : undefined,
            totalLosses: !isWinner ? { increment: 1 } : undefined,
            lastActiveAt: new Date(),
          }
        });
      }

      return game;
    }),

  // Mettre en pause une partie
  pause: procedure
    .input(z.object({ 
      id: z.number(),
      currentState: z.string()
    }))
    .mutation(async ({ ctx, input }) => {
      return await ctx.prisma.game.update({
        where: { id: input.id },
        data: {
          status: GameStatus.PAUSED,
          currentState: input.currentState,
          pausedAt: new Date(),
          updatedAt: new Date(),
        }
      });
    }),

  // Reprendre une partie
  resume: procedure
    .input(z.object({ id: z.number() }))
    .mutation(async ({ ctx, input }) => {
      return await ctx.prisma.game.update({
        where: { id: input.id },
        data: {
          status: GameStatus.IN_PROGRESS,
          pausedAt: null,
          updatedAt: new Date(),
        },
        include: {
          participants: {
            include: {
              player: true
            },
            orderBy: {
              order: 'asc'
            }
          }
        }
      });
    }),

  // Abandonner une partie
  abandon: procedure
    .input(z.object({ id: z.number() }))
    .mutation(async ({ ctx, input }) => {
      return await ctx.prisma.game.update({
        where: { id: input.id },
        data: {
          status: GameStatus.ABANDONED,
          finishedAt: new Date(),
          updatedAt: new Date(),
        }
      });
    }),

  // Récupérer l'historique des parties
  getHistory: procedure
    .input(z.object({
      playerId: z.number().optional(),
      limit: z.number().min(1).max(100).default(20),
      offset: z.number().default(0),
    }))
    .query(async ({ ctx, input }) => {
      const where: Record<string, unknown> = {
        status: {
          in: [GameStatus.FINISHED, GameStatus.ABANDONED]
        }
      };
      
      if (input.playerId) {
        where.participants = {
          some: {
            playerId: input.playerId
          }
        };
      }

      return await ctx.prisma.game.findMany({
        where,
        include: {
          participants: {
            include: {
              player: true
            },
            orderBy: {
              order: 'asc'
            }
          },
          winner: true,
          _count: {
            select: {
              turns: true
            }
          }
        },
        orderBy: {
          finishedAt: 'desc'
        },
        take: input.limit,
        skip: input.offset,
      });
    }),

  // Mettre à jour l'état d'une partie en cours
  updateState: procedure
    .input(z.object({
      id: z.number(),
      currentState: z.string(),
      status: z.nativeEnum(GameStatus).optional(),
    }))
    .mutation(async ({ ctx, input }) => {
      return await ctx.prisma.game.update({
        where: { id: input.id },
        data: {
          currentState: input.currentState,
          ...(input.status && { status: input.status }),
        }
      });
    }),

  // Supprimer une partie
  delete: procedure
    .input(z.object({ id: z.number() }))
    .mutation(async ({ ctx, input }) => {
      return await ctx.prisma.game.delete({
        where: { id: input.id }
      });
    }),
});