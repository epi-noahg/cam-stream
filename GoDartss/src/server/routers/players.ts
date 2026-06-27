/**
 * Routeur tRPC pour la gestion des joueurs
 */

import { z } from 'zod';
import { router, procedure } from '../trpc';

export const playersRouter = router({
  // Créer un nouveau joueur
  create: procedure
    .input(z.object({
      nickname: z.string().min(1).max(50),
      email: z.string().email().optional(),
      avatar: z.string().url().optional(),
    }))
    .mutation(async ({ ctx, input }) => {
      const player = await ctx.prisma.player.create({
        data: {
          nickname: input.nickname,
          email: input.email,
          avatar: input.avatar,
          stats: {
            create: {} // Créer des stats vides pour le joueur
          }
        },
        include: {
          stats: true,
          badges: true,
        }
      });
      return player;
    }),

  // Récupérer tous les joueurs
  getAll: procedure
    .query(async ({ ctx }) => {
      return await ctx.prisma.player.findMany({
        include: {
          stats: true,
          badges: {
            include: {
              // Optionnel: inclure des métadonnées du badge
            }
          },
          _count: {
            select: {
              wonGames: true,
              participants: true,
            }
          }
        },
        orderBy: {
          lastActiveAt: 'desc'
        }
      });
    }),

  // Récupérer un joueur par ID
  getById: procedure
    .input(z.object({ id: z.number() }))
    .query(async ({ ctx, input }) => {
      return await ctx.prisma.player.findUnique({
        where: { id: input.id },
        include: {
          stats: true,
          badges: true,
          wonGames: {
            include: {
              participants: {
                include: {
                  player: true
                }
              }
            },
            orderBy: {
              finishedAt: 'desc'
            },
            take: 10 // Dernières 10 victoires
          },
          participants: {
            include: {
              game: {
                include: {
                  participants: {
                    include: {
                      player: true
                    }
                  }
                }
              }
            },
            orderBy: {
              game: {
                createdAt: 'desc'
              }
            },
            take: 20 // Dernières 20 parties
          }
        }
      });
    }),

  // Mettre à jour un joueur
  update: procedure
    .input(z.object({
      id: z.number(),
      nickname: z.string().min(1).max(50).optional(),
      email: z.string().email().optional(),
      avatar: z.string().url().optional(),
    }))
    .mutation(async ({ ctx, input }) => {
      const { id, ...updateData } = input;
      return await ctx.prisma.player.update({
        where: { id },
        data: {
          ...updateData,
          lastActiveAt: new Date(),
        },
        include: {
          stats: true,
          badges: true,
        }
      });
    }),

  // Récupérer le classement des joueurs
  getLeaderboard: procedure
    .input(z.object({
      sortBy: z.enum(['wins', 'winRate', 'avgScore', 'total180s']).default('wins'),
      limit: z.number().min(1).max(100).default(20),
    }))
    .query(async ({ ctx, input }) => {
      const players = await ctx.prisma.player.findMany({
        include: {
          stats: true,
          _count: {
            select: {
              wonGames: true,
              participants: true,
            }
          }
        },
        where: {
          totalGames: {
            gt: 0 // Seulement les joueurs qui ont joué
          }
        },
        take: input.limit,
      });

      // Tri côté application selon le critère
      return players
        .map(player => ({
          ...player,
          winRate: player.totalGames > 0 ? (player.totalWins / player.totalGames) * 100 : 0,
        }))
        .sort((a, b) => {
          switch (input.sortBy) {
            case 'wins':
              return b.totalWins - a.totalWins;
            case 'winRate':
              return b.winRate - a.winRate;
            case 'avgScore':
              return (b.stats?.averageScore || 0) - (a.stats?.averageScore || 0);
            case 'total180s':
              return (b.stats?.total180s || 0) - (a.stats?.total180s || 0);
            default:
              return b.totalWins - a.totalWins;
          }
        });
    }),

  // Supprimer un joueur
  delete: procedure
    .input(z.object({ id: z.number() }))
    .mutation(async ({ ctx, input }) => {
      // Supprimer d'abord les statistiques et badges
      await ctx.prisma.playerStats.deleteMany({
        where: { playerId: input.id }
      });
      
      await ctx.prisma.playerBadge.deleteMany({
        where: { playerId: input.id }
      });

      return await ctx.prisma.player.delete({
        where: { id: input.id }
      });
    }),
});