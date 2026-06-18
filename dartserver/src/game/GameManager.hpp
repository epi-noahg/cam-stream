#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// GameManager — the single authoritative owner of the live match.
//
// It wraps the pure X01 engine with the match-level bookkeeping the front-end
// reducer used to do (winner / finishedPlayers / gameOver, undo history) and
// exposes a small command surface that both the detection pipeline and the
// WebSocket clients drive.  It is dependency-light (engine + std) and notifies
// an observer on every state change so the API layer can broadcast.
//
// Thread-safety: all mutating commands take an internal lock, so detection
// (callback thread) and the WS server (its own threads) can call concurrently.
// ─────────────────────────────────────────────────────────────────────────────

#include "GameTypes.hpp"

#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace dart::game {

/// Origin of a recorded throw, surfaced to the UI so it can flag darts that
/// came from the (fallible) vision pipeline and may need review.
struct ThrowMeta {
    bool        detected    {false};  // came from the camera pipeline
    float       confidence  {1.0f};   // [0..1]; 1.0 for manual input
    bool        needsReview {false};  // low-confidence detection
    std::string zone;                 // raw detector label, e.g. "T20"
};

class GameManager {
public:
    using StateCallback = std::function<void(const GameState&)>;
    /// Fired right after a detected dart is recorded, so the API can emit a
    /// dedicated `dart_detected` event in addition to the full state.
    using DetectedCallback =
        std::function<void(const Throw&, const ThrowMeta&, int dartIndexBefore)>;

    void setOnChanged(StateCallback cb)   { on_changed_  = std::move(cb); }
    void setOnDetected(DetectedCallback cb){ on_detected_ = std::move(cb); }
    /// Edge-triggered: fired once when the match transitions to gameOver, so a
    /// persistence layer can record the finished game.
    void setOnGameOver(StateCallback cb)  { on_game_over_ = std::move(cb); }

    // ── Lifecycle ────────────────────────────────────────────────────────
    void createGame(std::vector<PlayerState> players, const OptionsX01& opts);
    bool hasGame() const;
    /// Stable id of the current match (for resumable persistence).
    std::string gameId() const;
    /// Replace the live match with a previously saved state (resume).
    void loadState(GameState state, const std::string& id);

    // ── Commands (mutating) ──────────────────────────────────────────────
    /// Manual throw from the UI (full confidence).
    void recordManualThrow(const Throw& thr);
    /// Throw produced by the detection pipeline.
    void recordDetectedThrow(const Throw& thr, const ThrowMeta& meta);
    /// Correct a previously recorded throw; replays the match to stay coherent.
    void correctThrow(int turnIndex, int throwIndex, const Throw& newThrow);
    /// Undo the last mutation.
    void undo();
    /// Force a hand-over to the next active player (e.g. cams missed a dart).
    void nextPlayer();

    // ── Reads ────────────────────────────────────────────────────────────
    GameState snapshot() const;
    std::optional<std::vector<Throw>> currentCheckout() const;

private:
    void recordThrowLocked_(const Throw& thr);  // shared core, lock held
    void pushHistoryLocked_();
    void notify_(const GameState& s);

    mutable std::mutex     mtx_;
    GameState              state_;
    std::vector<GameState> history_;        // for undo (bounded)
    bool                   has_game_ {false};
    std::string            game_id_;
    StateCallback          on_changed_;
    DetectedCallback       on_detected_;
    StateCallback          on_game_over_;

    static constexpr std::size_t MAX_HISTORY = 40;
};

} // namespace dart::game
