#!/usr/bin/env tsx

/**
 * Script pour peupler la base de données avec des données de test réalistes
 * Usage: pnpm db:seed
 */

import { PrismaClient } from '@prisma/client';

const prisma = new PrismaClient();

async function seedDatabase() {
  console.log('🌱 Peuplement de la base de données...');

  try {
    // Créer des joueurs avec des statistiques variées
    const players = await Promise.all([
      // Joueur expert
      prisma.player.create({
        data: {
          nickname: 'ProDart',
          email: 'pro@dart.com',
          totalGames: 25,
          totalWins: 18,
          totalLosses: 7,
          bestFinish: 170,
          stats: {
            create: {
              averageScore: 68.5,
              totalThrows: 450,
              totalScore: 30825,
              bull50Count: 8,
              bull25Count: 4,
              tripleCount: 28,
              doubleCount: 35,
              checkoutAttempts: 25,
              checkoutSuccess: 18,
              averageCheckoutScore: 87.3,
              bestThreeDartAverage: 180,
              longest180Series: 3,
              total180s: 8,
            }
          }
        }
      }),
      
      // Joueur intermédiaire  
      prisma.player.create({
        data: {
          nickname: 'MidPlayer',
          email: 'mid@dart.com',
          totalGames: 15,
          totalWins: 7,
          totalLosses: 8,
          bestFinish: 121,
          stats: {
            create: {
              averageScore: 45.2,
              totalThrows: 285,
              totalScore: 12882,
              bull50Count: 3,
              bull25Count: 2,
              tripleCount: 12,
              doubleCount: 18,
              checkoutAttempts: 15,
              checkoutSuccess: 7,
              averageCheckoutScore: 65.4,
              bestThreeDartAverage: 140,
              longest180Series: 1,
              total180s: 2,
            }
          }
        }
      }),
      
      // Débutant
      prisma.player.create({
        data: {
          nickname: 'Newbie',
          email: 'new@dart.com',
          totalGames: 8,
          totalWins: 2,
          totalLosses: 6,
          bestFinish: 84,
          stats: {
            create: {
              averageScore: 28.7,
              totalThrows: 156,
              totalScore: 4477,
              bull50Count: 0,
              bull25Count: 1,
              tripleCount: 3,
              doubleCount: 8,
              checkoutAttempts: 8,
              checkoutSuccess: 2,
              averageCheckoutScore: 42.0,
              bestThreeDartAverage: 95,
              longest180Series: 0,
              total180s: 0,
            }
          }
        }
      }),
      
      // Joueur régulier
      prisma.player.create({
        data: {
          nickname: 'RegularJoe',
          email: 'joe@dart.com',
          totalGames: 20,
          totalWins: 9,
          totalLosses: 11,
          bestFinish: 98,
          stats: {
            create: {
              averageScore: 38.4,
              totalThrows: 360,
              totalScore: 13824,
              bull50Count: 2,
              bull25Count: 1,
              tripleCount: 8,
              doubleCount: 22,
              checkoutAttempts: 20,
              checkoutSuccess: 9,
              averageCheckoutScore: 56.7,
              bestThreeDartAverage: 121,
              longest180Series: 0,
              total180s: 1,
            }
          }
        }
      }),

      // Vos joueurs de test habituels
      prisma.player.create({
        data: {
          nickname: 'Test1',
          email: 'test1@example.com',
          totalGames: 0,
          totalWins: 0,
          totalLosses: 0,
          stats: {
            create: {
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
          }
        }
      }),
      
      prisma.player.create({
        data: {
          nickname: 'Test2',
          email: 'test2@example.com',
          totalGames: 0,
          totalWins: 0,
          totalLosses: 0,
          stats: {
            create: {
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
          }
        }
      })
    ]);

    console.log(`✅ ${players.length} joueurs créés avec leurs statistiques`);

    // Créer quelques parties d'exemple terminées
    const finishedGame = await prisma.game.create({
      data: {
        mode: 'X01',
        status: 'FINISHED',
        startingScore: 501,
        doubleOut: true,
        doubleIn: false,
        winnerId: players[0].id, // ProDart gagne
        winnerRank: [players[0].id, players[1].id],
        finishedAt: new Date(Date.now() - 1000 * 60 * 60 * 2), // Il y a 2h
        participants: {
          create: [
            { playerId: players[0].id, order: 0 },
            { playerId: players[1].id, order: 1 }
          ]
        }
      }
    });

    console.log('✅ Partie d\'exemple créée');

    // Créer une partie en cours
    const activeGame = await prisma.game.create({
      data: {
        mode: 'X01',
        status: 'IN_PROGRESS',
        startingScore: 501,
        doubleOut: true,
        doubleIn: false,
        currentState: JSON.stringify({
          options: { startingScore: 501, outType: 'DOUBLE', inType: 'ANY' },
          players: [
            { id: players[4].id, nickname: 'Test1', score: 350, throws: [] },
            { id: players[5].id, nickname: 'Test2', score: 420, throws: [] }
          ],
          currentIndex: 0,
          dartIndex: 0,
          turns: [[]],
          winner: null,
          finishedPlayers: [],
          gameOver: false
        }),
        participants: {
          create: [
            { playerId: players[4].id, order: 0 },
            { playerId: players[5].id, order: 1 }
          ]
        }
      }
    });

    console.log('✅ Partie en cours créée');

    console.log('\n🎯 Base de données peuplée avec succès !');
    console.log('📊 Résumé:');
    console.log(`   • ${players.length} joueurs avec statistiques variées`);
    console.log(`   • 1 partie terminée`);
    console.log(`   • 1 partie en cours pour tester la reprise`);

  } catch (error) {
    console.error('❌ Erreur lors du peuplement:', error);
    process.exit(1);
  } finally {
    await prisma.$disconnect();
  }
}

seedDatabase();