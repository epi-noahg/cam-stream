#include "GameManager.hpp"

#include "X01Engine.hpp"
#include "CricketEngine.hpp"
#include "RoundClockEngine.hpp"
#include "Checkout.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace dart::game {
namespace {

std::string makeGameId() {
    // Process-local monotonic counter guarantees uniqueness even when several
    // games are created within the same millisecond (the timestamp alone would
    // collide and the saved-game upsert would overwrite the earlier match).
    static std::atomic<std::uint64_t> seq{0};
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "g" + std::to_string(ms) + "-" +
           std::to_string(seq.fetch_add(1, std::memory_order_relaxed));
}

bool contains(const std::vector<int>& v, int id) {
    for (int x : v) if (x == id) return true;
    return false;
}

const PlayerState* findById(const GameState& s, int id) {
    for (const PlayerState& p : s.players) if (p.id == id) return &p;
    return nullptr;
}

/// Team identity: solo players (team 0) are their own team.
int teamKey(const PlayerState& p) { return p.team == 0 ? -(p.id + 1) : p.team; }

/// Distinct team keys among players that haven't finished.
std::vector<int> activeTeamKeys(const GameState& s) {
    std::vector<int> out;
    for (const PlayerState& p : s.players) {
        if (contains(s.finishedPlayers, p.id)) continue;
        const int k = teamKey(p);
        if (!contains(out, k)) out.push_back(k);
    }
    return out;
}

int indexOfPlayer(const GameState& s, int id) {
    for (int i = 0; i < static_cast<int>(s.players.size()); ++i)
        if (s.players[i].id == id) return i;
    return 0;
}

/// Reset a player to the starting state for the chosen mode.
void initPlayerForMode(PlayerState& p, const GameState& s) {
    p.legsWon = 0;
    p.throws.clear();
    p.marks.clear();
    p.target = 0;
    switch (s.mode) {
        case GameMode::X01:
            p.score = s.options.startingScore;
            break;
        case GameMode::Cricket:
            p.score = 0;
            p.marks.assign(7, 0);
            break;
        case GameMode::RoundTheClock:
            p.score  = 0;
            p.target = 1;
            break;
    }
}

/// Dispatch a single throw to the engine for the active mode.
ApplyResult applyForMode(const GameState& s, const Throw& thr) {
    switch (s.mode) {
        case GameMode::Cricket:       return applyCricketThrow(s, thr);
        case GameMode::RoundTheClock: return applyRoundClockThrow(s, thr);
        case GameMode::X01:           break;
    }
    return applyThrow(s, thr);
}

/// Dispatch a full replay to the engine for the active mode.
GameState recalcForMode(const GameState& s) {
    switch (s.mode) {
        case GameMode::Cricket:       return recalculateCricket(s);
        case GameMode::RoundTheClock: return recalculateRoundClock(s);
        case GameMode::X01:           break;
    }
    return recalculateFromTurns(s);
}

} // namespace

void GameManager::createGame(std::vector<PlayerState> players,
                             const GameConfig& cfg) {
    GameState copy;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        state_ = GameState{};
        state_.mode       = cfg.mode;
        state_.options    = cfg.x01;
        state_.cricket    = cfg.cricket;
        state_.roundClock = cfg.roundClock;
        state_.players = std::move(players);
        for (PlayerState& p : state_.players)
            initPlayerForMode(p, state_);
        state_.currentIndex = 0;
        state_.dartIndex    = 0;
        state_.turns        = {{}};
        state_.winner.reset();
        state_.finishedPlayers.clear();
        state_.gameOver = false;
        history_.clear();
        has_game_ = true;
        game_id_ = makeGameId();
        copy = state_;
    }
    if (on_changed_) on_changed_(copy);  // outside the lock (callbacks may re-enter)
}

void GameManager::createGame(std::vector<PlayerState> players,
                             const OptionsX01& opts) {
    GameConfig cfg;
    cfg.mode = GameMode::X01;
    cfg.x01  = opts;
    createGame(std::move(players), cfg);
}

bool GameManager::hasGame() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return has_game_;
}

std::string GameManager::gameId() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return game_id_;
}

void GameManager::loadState(GameState state, const std::string& id) {
    GameState copy;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        state_ = std::move(state);
        if (state_.turns.empty()) state_.turns.push_back({});
        history_.clear();
        has_game_ = true;
        game_id_ = id;
        copy = state_;
    }
    if (on_changed_) on_changed_(copy);
}

void GameManager::pushHistoryLocked_() {
    history_.push_back(state_);
    if (history_.size() > MAX_HISTORY)
        history_.erase(history_.begin(), history_.end() - MAX_HISTORY);
}

// Shared core: apply one throw and reconcile match-level winner / finished /
// gameOver bookkeeping.  Port of the THROW case in GoDartss GameContext.tsx.
void GameManager::recordThrowLocked_(const Throw& thr) {
    if (!has_game_ || state_.gameOver) return;
    pushHistoryLocked_();

    const int playerId = state_.players[state_.currentIndex].id;
    ApplyResult res = applyForMode(state_, thr);
    GameState ns = std::move(res.state);

    if (res.finished && !contains(ns.finishedPlayers, playerId)) {
        // The whole team finishes together.
        const PlayerState* fp = findById(ns, playerId);
        const int finishedTeam = fp ? teamKey(*fp) : 0;
        for (const PlayerState& p : ns.players)
            if (teamKey(p) == finishedTeam && !contains(ns.finishedPlayers, p.id))
                ns.finishedPlayers.push_back(p.id);

        // Cricket / Round the Clock end the moment someone meets the win
        // condition; X01 keeps the rest playing for placings.
        if (ns.mode != GameMode::X01) {
            if (!ns.winner.has_value()) ns.winner = playerId;
            for (const PlayerState& p : ns.players)
                if (!contains(ns.finishedPlayers, p.id))
                    ns.finishedPlayers.push_back(p.id);
            ns.gameOver = true;
            state_ = std::move(ns);
            return;
        }

        if (!ns.winner.has_value() && fp && fp->legsWon >= ns.options.legs)
            ns.winner = playerId;

        // Game is over when at most one team is still in.
        const std::vector<int> remainingTeams = activeTeamKeys(ns);
        if (remainingTeams.size() <= 1) {
            ns.gameOver = true;
            if (remainingTeams.size() == 1)
                for (const PlayerState& p : ns.players)
                    if (teamKey(p) == remainingTeams[0] && !contains(ns.finishedPlayers, p.id))
                        ns.finishedPlayers.push_back(p.id);
            if (!ns.winner.has_value() && !ns.finishedPlayers.empty())
                ns.winner = ns.finishedPlayers[0];
        } else if (contains(ns.finishedPlayers,
                            ns.players[ns.currentIndex].id)) {
            // Current player just finished → advance to the next active one.
            const int n = static_cast<int>(ns.players.size());
            int idx = (ns.currentIndex + 1) % n;
            for (int a = 0; a < n; ++a) {
                if (!contains(ns.finishedPlayers, ns.players[idx].id)) {
                    ns.currentIndex = idx;
                    ns.dartIndex    = 0;
                    break;
                }
                idx = (idx + 1) % n;
            }
        }
    }

    state_ = std::move(ns);
}

void GameManager::recordManualThrow(const Throw& thr) {
    GameState copy;
    bool wasOver;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        wasOver = state_.gameOver;
        recordThrowLocked_(thr);
        copy = state_;
    }
    if (on_changed_) on_changed_(copy);
    if (!wasOver && copy.gameOver && on_game_over_) on_game_over_(copy);
}

void GameManager::recordDetectedThrow(const Throw& thr, const ThrowMeta& meta) {
    GameState copy;
    int dartIndexBefore;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!has_game_ || state_.gameOver) return;
        dartIndexBefore = state_.dartIndex;
        recordThrowLocked_(thr);
        copy = state_;
    }
    if (on_detected_) on_detected_(thr, meta, dartIndexBefore);
    if (on_changed_) on_changed_(copy);
    if (copy.gameOver && on_game_over_) on_game_over_(copy);
}

void GameManager::correctThrow(int turnIndex, int throwIndex,
                               const Throw& newThrow) {
    GameState copy;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!has_game_) return;
        if (turnIndex < 0 || turnIndex >= static_cast<int>(state_.turns.size()))
            return;
        if (throwIndex < 0 ||
            throwIndex >= static_cast<int>(state_.turns[turnIndex].size()))
            return;
        pushHistoryLocked_();

        state_.turns[turnIndex][throwIndex] = newThrow;
        GameState rec = recalcForMode(state_);

        if (rec.mode == GameMode::X01) {
            // Rebuild winner / finished / gameOver from the replayed state.
            rec.finishedPlayers.clear();
            for (const PlayerState& p : rec.players)
                if (p.legsWon >= rec.options.legs)
                    rec.finishedPlayers.push_back(p.id);
            rec.winner = rec.finishedPlayers.empty()
                ? std::optional<int>{}
                : std::optional<int>{rec.finishedPlayers.front()};
            rec.gameOver = rec.finishedPlayers.size() >= rec.players.size() - 1 &&
                           !rec.players.empty();
        }
        // For Cricket / Round the Clock the replay already cleared the
        // match-level flags; an edited game simply continues from the new state.

        state_ = std::move(rec);
        copy = state_;
    }
    if (on_changed_) on_changed_(copy);
}

void GameManager::undo() {
    GameState copy;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (history_.empty()) return;
        state_ = history_.back();
        history_.pop_back();
        copy = state_;
    }
    if (on_changed_) on_changed_(copy);
}

void GameManager::nextPlayer() {
    GameState copy;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!has_game_ || state_.gameOver || state_.players.empty()) return;
        pushHistoryLocked_();

        const int n = static_cast<int>(state_.players.size());
        int idx = (state_.currentIndex + 1) % n;
        for (int a = 0; a < n; ++a) {
            if (!contains(state_.finishedPlayers, state_.players[idx].id)) break;
            idx = (idx + 1) % n;
        }
        state_.currentIndex = idx;
        state_.dartIndex    = 0;
        if (state_.turns.empty() || !state_.turns.back().empty())
            state_.turns.push_back({});
        copy = state_;
    }
    if (on_changed_) on_changed_(copy);
    (void)indexOfPlayer;  // reserved helper
}

GameState GameManager::snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return state_;
}

std::optional<std::vector<Throw>> GameManager::currentCheckout() const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!has_game_ || state_.players.empty()) return std::nullopt;
    if (state_.mode != GameMode::X01) return std::nullopt;  // checkout is X01-only
    const PlayerState& p = state_.players[state_.currentIndex];
    const int dartsLeft = 3 - state_.dartIndex;
    return getCheckoutSuggestion(p.score, dartsLeft, state_.options.outType);
}

void GameManager::notify_(const GameState& s) {
    if (on_changed_) on_changed_(s);
}

} // namespace dart::game
