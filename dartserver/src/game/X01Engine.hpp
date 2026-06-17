#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Pure X01 scoring rules — a faithful C++ port of GoDartss/src/lib/x01.ts and
// the recalc logic in src/context/GameContext.tsx.  No I/O, no dependencies:
// every function is a pure transform over GameState, which makes the rules
// trivially unit-testable and keeps the authoritative server deterministic.
// ─────────────────────────────────────────────────────────────────────────────

#include "GameTypes.hpp"

namespace dart::game {

struct ApplyResult {
    GameState state;
    bool      bust     {false};
    bool      finished {false};
};

/// Apply one throw to @p state for the current player, returning the next
/// state plus whether it busted / finished the leg.  Mirrors applyThrow().
ApplyResult applyThrow(const GameState& state, const Throw& thr);

/// Replay every turn from scratch to recompute scores after an edit, skipping
/// any turn that contains a bust (matching recalculateScoresFromTurns()).
/// Used by GameManager when a throw is corrected.
GameState recalculateFromTurns(const GameState& state);

} // namespace dart::game
