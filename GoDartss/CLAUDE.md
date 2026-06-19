# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

GoDarts is a Next.js-based digital dartboard application that simulates X01 dart games (301/501/701). The app features real-time scoring, turn management, and winner celebration with support for multiple players.

## Technology Stack

- **Framework**: Next.js 15.4.4 with App Router
- **Language**: TypeScript 5
- **Database**: SQLite with Prisma ORM
- **State Management**: React Context + useReducer pattern with Zustand
- **Real-time**: Socket.io for live game updates
- **Styling**: Tailwind CSS 4 with unified theme system
- **Authentication**: NextAuth.js
- **API**: tRPC for type-safe API endpoints
- **Testing**: Vitest

## Development Commands

```bash
# Development server
pnpm dev

# Build production
pnpm build

# Type checking
pnpm types

# Linting
pnpm lint

# Testing
pnpm test

# Database operations
pnpm db:migrate    # Run migrations
pnpm db:generate   # Generate Prisma client
```

## Architecture Overview

### Game State Management
The application uses a React Context pattern with useReducer for complex game state management:

- **GameContext** (`src/context/GameContext.tsx`): Centralized game state with reducer pattern
- **Game Types** (`src/types/game.ts`): TypeScript types for all game entities
- **X01 Logic** (`src/lib/x01.ts`): Core dart game logic and scoring rules

### Key State Flow
1. Game setup creates initial `GameState` with players and options
2. `applyThrow()` function processes dart throws and updates scores
3. Reducer handles actions: `THROW`, `UNDO`, `EDIT_THROW`, `NEXT_PLAYER`
4. Context provides state and dispatch to all game components

### Database Schema
- **Player**: User profiles with nicknames
- **Game**: Game sessions with mode and configuration
- **GameParticipant**: Junction table for players in games
- **Turn**: Individual player turns in a game
- **Throw**: Individual dart throws with value and multiplier

### Component Structure
- **Game Components**: `Dartboard.tsx`, `Scoreboard.tsx`, `EditableScoreboard.tsx`
- **UI Components**: Located in `src/components/ui/` following shadcn/ui patterns
- **Game Pages**: X01 setup (`src/app/x01/setup/`) and play (`src/app/x01/play/[gameId]/`)

## Theme System

The application uses a unified theme system based on **Black, White, Red, and Gray** palette:

### Theme Usage
```typescript
import { useTheme, useGameTheme } from '@/hooks/useTheme';
import { THEME_CLASSES } from '@/lib/theme';

// In components
const theme = useTheme();
className={theme.common.card}
className={theme.getButton('primary')}
```

### Theme Files
- `src/lib/theme.ts`: Core theme constants and utilities
- `src/lib/theme-fixes.ts`: Component-specific theme corrections
- `src/hooks/useTheme.ts`: React hooks for theme usage
- `THEME-SYSTEM.md`: Complete theme documentation

### Color Standards
- **Primary Actions**: Red (`bg-red-600`, `text-red-600`)
- **Backgrounds**: Black for main areas, white for cards
- **Text**: White on dark backgrounds, black on light
- **Secondary**: Gray scale for borders and inactive states
- **Hover States**: Use `hover:bg-gray-100` (not `bg-gray-50`) with black text for proper contrast

## Game Logic Patterns

### X01 Scoring Rules
- Players start with configurable score (301/501/701)
- Support for Double In/Out requirements
- Bust detection when score goes negative or equals 1
- Winner determination based on first to reach exactly 0

### Turn Management
- 3 darts per turn maximum
- Automatic player rotation after turn completion
- Support for finished players (continue game with remaining players)
- Undo functionality that recalculates entire game state

### State Recalculation
When editing throws, the entire game state is recalculated from scratch by replaying all turns to ensure consistency.

## API Structure

The app uses tRPC for type-safe APIs:
- **Server**: `src/server/` contains tRPC router definitions
- **Client**: React Query integration for data fetching
- **Real-time**: Socket.io for live game updates

## Testing Approach

Run tests with `pnpm test` using Vitest. Focus testing on:
- Game logic functions in `src/lib/x01.ts`
- State management in GameContext
- Component behavior for score calculations

## Development Guidelines

### Code Style
- Use TypeScript strictly with proper typing
- Follow the established React Context + Reducer pattern for state
- Maintain consistency with the unified theme system
- Use Prisma for all database operations

### Component Patterns
- Game components should use `useGame()` hook to access state
- UI components follow shadcn/ui patterns in `src/components/ui/`
- Theme application through `useTheme()` hooks
- Form validation with Zod schemas

### Game State Updates
- Always use the reducer dispatch for state changes
- Use `applyThrow()` for scoring logic
- Implement undo by replaying game state
- Validate game rules in the core logic functions

## File Organization

- `/src/app/`: Next.js App Router pages and layouts
- `/src/components/`: Reusable React components
- `/src/context/`: React Context providers
- `/src/hooks/`: Custom React hooks
- `/src/lib/`: Utility functions and core game logic
- `/src/server/`: tRPC server setup and routers
- `/src/types/`: TypeScript type definitions
- `/prisma/`: Database schema and migrations