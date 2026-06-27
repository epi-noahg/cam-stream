#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Pure game-domain types for the authoritative darts server.
//
// These mirror the front-end shapes in GoDartss/src/types/game.ts 1:1 so the
// JSON we broadcast maps directly onto what the UI already renders.  This
// header has NO external dependencies (no OpenCV / no JSON lib) — the game
// engine is pure and unit-testable on its own.
// ─────────────────────────────────────────────────────────────────────────────

#include <optional>
#include <string>
#include <vector>

namespace dart::game {

/// In/out rule for opening and finishing a leg.
enum class InOutType { Any, Double, Triple };

/// Match format (kept for parity with the front; single-leg is the default).
enum class ScoringMode { BestOf, FirstTo };

/// Which game is being played.  X01 is the historical default; Cricket and
/// RoundTheClock are scored by their own engines but share all the match-level
/// bookkeeping (turns, teams, undo, persistence).
enum class GameMode { X01, Cricket, RoundTheClock };

/// A single dart throw.
///   value      1..20, or 25 (outer bull) / 50 (inner bull)
///   multiplier 1 (single), 2 (double), 3 (triple); always 1 for the bulls
///   bust       true when this throw caused a bust (kept for history/display)
struct Throw {
    int  value      {0};
    int  multiplier {1};
    bool bust       {false};

    /// Points this throw is worth (bulls ignore the multiplier).
    int hitValue() const {
        return (value == 25 || value == 50) ? value : value * multiplier;
    }

    bool operator==(const Throw& o) const {
        return value == o.value && multiplier == o.multiplier && bust == o.bust;
    }
};

/// X01-specific configuration for a match.
struct OptionsX01 {
    int         startingScore {501};
    InOutType   inType        {InOutType::Any};
    InOutType   outType       {InOutType::Double};
    ScoringMode scoringMode   {ScoringMode::FirstTo};
    int         legs          {1};
    bool        allowBust     {false};
    int         teams         {1};
};

/// Cricket configuration.  Targets are 15..20 plus the bull; three marks close a
/// number.  In the standard variant the closer banks points; in cut-throat the
/// points are dumped onto opponents who have not yet closed that number.
struct OptionsCricket {
    bool        cutThroat   {false};
    bool        useBull     {true};
    ScoringMode scoringMode {ScoringMode::FirstTo};
    int         legs        {1};
    int         teams       {1};
};

/// Round the Clock configuration.  Players race through the numbers 1..20 in
/// order; any hit on the current target advances by one.
struct OptionsRoundClock {
    ScoringMode scoringMode {ScoringMode::FirstTo};
    int         legs        {1};
    int         teams       {1};
};

/// Canonical Cricket target order (matches `PlayerState::marks` indexing).
/// Index 6 is the bull (25 outer / 50 inner).
inline constexpr int kCricketTargets[7] = {20, 19, 18, 17, 16, 15, 25};

/// Per-player state within a match.
///
/// `team` groups players sharing a single score (real team X01).  0 means
/// "solo" — the player is their own team.  Teammates always share `score` and
/// `legsWon`; each keeps an individual throw history.
struct PlayerState {
    int                id       {0};
    std::string        nickname;
    int                score    {0};
    int                legsWon  {0};
    int                team     {0};
    std::vector<Throw> throws;

    // ── Mode-specific state (unused/empty outside the owning mode) ──────────
    /// Cricket: 7 entries (0..3 marks) indexed by `kCricketTargets`.  Cricket
    /// points reuse `score`.  Empty for non-Cricket games.
    std::vector<int>   marks;
    /// Round the Clock: current number to hit (1..20; 21 means finished).
    int                target   {0};
};

/// Authoritative state of a single match.  Held by GameManager and serialized
/// verbatim to clients.  Undo history is kept by the manager, not here.
struct GameState {
    GameMode                         mode {GameMode::X01};
    OptionsX01                       options;     // meaningful when mode == X01
    OptionsCricket                   cricket;     // meaningful when mode == Cricket
    OptionsRoundClock                roundClock;  // meaningful when mode == RoundTheClock
    std::vector<PlayerState>         players;
    int                              currentIndex {0};
    int                              dartIndex    {0};
    std::vector<std::vector<Throw>>  turns        {{}};  // start with one empty turn
    std::optional<int>               winner;             // player id of leg winner
    std::vector<int>                 finishedPlayers;    // ids, in finishing order
    bool                             gameOver     {false};
};

} // namespace dart::game
