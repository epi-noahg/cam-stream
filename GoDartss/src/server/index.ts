/**
 * Routeur principal tRPC combinant tous les sous-routeurs
 */

import { router } from './trpc';
import { playersRouter } from './routers/players';
import { gamesRouter } from './routers/games';
import { turnsRouter } from './routers/turns';
import { badgesRouter } from './routers/badges';
import { statsRouter } from './routers/stats';

export const appRouter = router({
  player: playersRouter,
  game: gamesRouter,
  turn: turnsRouter,
  badge: badgesRouter,
  stat: statsRouter,
});

export type AppRouter = typeof appRouter;