#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Pure Cricket scoring rules.  Like X01Engine, every function is a pure
// transform over GameState — no I/O, no dependencies — so the rules are
// deterministic and trivially unit-testable.
//
// Targets are 20, 19, 18, 17, 16, 15 and the bull (see GameTypes::kCricketTargets).
// Three marks close a number.  A single scores 1 mark, a double 2, a triple 3;
// the outer bull (25) is 1 mark and the inner bull (50) is 2.  Marks beyond the
// three needed to close convert to points:
//   • standard  → added to the thrower's own score (highest score wins);
//   • cut-throat → added to every opponent who has not closed that number
//                  (lowest score wins).
// A player finishes when they have closed every target AND lead on points
// (standard) / trail on points (cut-throat).
// ─────────────────────────────────────────────────────────────────────────────

#include "GameTypes.hpp"
#include "X01Engine.hpp"  // for ApplyResult

namespace dart::game {

/// Apply one throw to @p state for the current player, returning the next state
/// plus whether it finished the match.  Cricket never busts, so `bust` is always
/// false; the field is kept for a uniform ApplyResult across engines.
ApplyResult applyCricketThrow(const GameState& state, const Throw& thr);

/// Replay every recorded turn from scratch to recompute marks/points after an
/// edit.  Used by GameManager when a throw is corrected.
GameState recalculateCricket(const GameState& state);

} // namespace dart::game
