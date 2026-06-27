#!/usr/bin/env tsx

/**
 * Script pour reset rapide de la base de données SANS confirmation
 * Usage: pnpm db:quick-reset
 * ⚠️  ATTENTION: Supprime tout sans demander !
 */

import { PrismaClient } from '@prisma/client';

const prisma = new PrismaClient();

async function quickReset() {
  console.log('🚀 Reset rapide de la base de données...');

  try {
    // Supprimer tout dans l'ordre
    await prisma.playerBadge.deleteMany();
    await prisma.throw.deleteMany();
    await prisma.turn.deleteMany();
    await prisma.gameParticipant.deleteMany();
    await prisma.playerStats.deleteMany();
    await prisma.game.deleteMany();
    await prisma.player.deleteMany();

    // Reset auto-increment
    await prisma.$executeRaw`DELETE FROM sqlite_sequence`;

    console.log('✅ Base de données vidée !');
    console.log('💡 Tip: Utilisez "pnpm db:reset --force" pour recréer des données de test');

  } catch (error) {
    console.error('❌ Erreur:', error);
    process.exit(1);
  } finally {
    await prisma.$disconnect();
  }
}

quickReset();