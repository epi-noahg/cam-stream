#!/usr/bin/env tsx

/**
 * Script pour reset complètement la base de données
 * Usage: pnpm db:reset
 */

import { PrismaClient } from '@prisma/client';
import fs from 'fs';
import path from 'path';

const prisma = new PrismaClient();

async function resetDatabase() {
  console.log('🗑️  Début du reset de la base de données...\n');

  try {
    // 1. Supprimer toutes les données dans l'ordre inverse des dépendances
    console.log('📊 Suppression des données...');
    
    // Tables avec foreign keys en premier
    await prisma.playerBadge.deleteMany();
    console.log('   ✅ PlayerBadge supprimés');
    
    await prisma.throw.deleteMany();
    console.log('   ✅ Throw supprimés');
    
    await prisma.turn.deleteMany();
    console.log('   ✅ Turn supprimés');
    
    await prisma.gameParticipant.deleteMany();
    console.log('   ✅ GameParticipant supprimés');
    
    await prisma.playerStats.deleteMany();
    console.log('   ✅ PlayerStats supprimés');
    
    // Tables principales
    await prisma.game.deleteMany();
    console.log('   ✅ Game supprimés');
    
    await prisma.player.deleteMany();
    console.log('   ✅ Player supprimés');

    console.log('\n✨ Toutes les données ont été supprimées !');

    // 2. Reset des sequences d'auto-increment (SQLite)
    console.log('\n🔄 Reset des compteurs auto-increment...');
    
    await prisma.$executeRaw`DELETE FROM sqlite_sequence WHERE name IN ('Player', 'Game', 'GameParticipant', 'Turn', 'Throw', 'PlayerBadge', 'PlayerStats')`;
    console.log('   ✅ Compteurs resetés');

    // 3. Optionnel: Recreer des données de test
    console.log('\n🌱 Création de données de test...');
    
    // Créer quelques joueurs de test
    const testPlayers = await Promise.all([
      prisma.player.create({
        data: {
          nickname: 'Mattéo',
          email: '',
          totalGames: 0,
          totalWins: 0,
          totalLosses: 0,
        }
      }),
      prisma.player.create({
        data: {
          nickname: 'Cedric',
          email: '',
          totalGames: 0,
          totalWins: 0,
          totalLosses: 0,
        }
      }),
      prisma.player.create({
        data: {
          nickname: 'Sandra',
          email: '',
          totalGames: 0,
          totalWins: 0,
          totalLosses: 0,
        }
      }),
      prisma.player.create({
        data: {
          nickname: 'Noah',
          email: '',
          totalGames: 0,
          totalWins: 0,
          totalLosses: 0,
        }
      })
    ]);

    console.log(`   ✅ ${testPlayers.length} joueurs de test créés`);

    // Créer les PlayerStats pour chaque joueur
    await Promise.all(testPlayers.map(player => 
      prisma.playerStats.create({
        data: {
          playerId: player.id,
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
      })
    ));

    console.log('   ✅ PlayerStats créés pour tous les joueurs');

    console.log('\n🎉 Reset de la base de données terminé avec succès !');
    console.log(`📈 Joueurs disponibles :`);
    testPlayers.forEach((player, index) => {
      console.log(`   ${index + 1}. ${player.nickname} (ID: ${player.id})`);
    });

  } catch (error) {
    console.error('❌ Erreur lors du reset :', error);
    process.exit(1);
  } finally {
    await prisma.$disconnect();
  }
}

// Confirmation avant reset
async function confirmReset() {
  if (process.argv.includes('--force')) {
    return true;
  }

  // En mode interactif, demander confirmation
  console.log('⚠️  ATTENTION: Cette action va supprimer TOUTES les données de la base !');
  console.log('🔴 Voulez-vous vraiment continuer ?');
  console.log('   Pour confirmer, relancez avec: pnpm db:reset --force');
  console.log('   Ou ajoutez --force à votre commande');
  
  return false;
}

async function main() {
  const confirmed = await confirmReset();
  
  if (!confirmed) {
    console.log('\n🛑 Reset annulé par sécurité');
    process.exit(0);
  }

  await resetDatabase();
}

main();