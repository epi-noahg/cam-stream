/**
 * Client tRPC pour le frontend avec React Query hooks
 */

import { createTRPCReact } from '@trpc/react-query';
import type { AppRouter } from '@/server';

export const trpc = createTRPCReact<AppRouter>();