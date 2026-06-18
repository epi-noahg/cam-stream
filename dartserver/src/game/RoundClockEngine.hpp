#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Pure Round the Clock scoring rules.  Players race through the numbers 1..20
// in order; any hit (single, double or triple) on the player's current target
// advances them by one.  The first to clear 20 wins.  Like the other engines
// this is a pure transform over GameState — no I/O, no dependencies.
// ─────────────────────────────────────────────────────────────────────────────

#include "GameTypes.hpp"
#include "X01Engine.hpp"  // for ApplyResult

namespace dart::game {

/// Apply one throw for the current player.  Round the Clock never busts, so
/// `bust` is always false; the field is kept for a uniform ApplyResult.
ApplyResult applyRoundClockThrow(const GameState& state, const Throw& thr);

/// Replay every recorded turn from scratch to recompute targets after an edit.
GameState recalculateRoundClock(const GameState& state);

} // namespace dart::game
