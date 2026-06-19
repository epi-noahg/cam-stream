/**
 * Configuration du serveur tRPC pour GoDarts
 */

import { initTRPC } from '@trpc/server';
import { PrismaClient } from '@prisma/client';
import { z } from 'zod';

// Initialisation de Prisma
const prisma = new PrismaClient();

// Cr�ation du contexte tRPC
export const createTRPCContext = () => {
  return {
    prisma,
  };
};

type Context = ReturnType<typeof createTRPCContext>;

// Initialisation de tRPC
const t = initTRPC.context<Context>().create();

// Exports pour les routeurs
export const router = t.router;
export const procedure = t.procedure;