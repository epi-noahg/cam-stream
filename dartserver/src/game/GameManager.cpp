#include "GameManager.hpp"

#include "X01Engine.hpp"
#include "Checkout.hpp"

namespace dart::game {
namespace {

bool contains(const std::vector<int>& v, int id) {
    for (int x : v) if (x == id) return true;
    return false;
}

const PlayerState* findById(const GameState& s, int id) {
    for (const PlayerState& p : s.players) if (p.id == id) return &p;
    return nullptr;
}

std::vector<int> activePlayerIds(const GameState& s) {
    std::vector<int> out;
    for (const PlayerState& p : s.players)
        if (!contains(s.finishedPlayers, p.id)) out.push_back(p.id);
    return out;
}

int indexOfPlayer(const GameState& s, int id) {
    for (int i = 0; i < static_cast<int>(s.players.size()); ++i)
        if (s.players[i].id == id) return i;
    return 0;
}

} // namespace

void GameManager::createGame(std::vector<PlayerState> players,
                             const OptionsX01& opts) {
    GameState copy;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        state_ = GameState{};
        state_.options = opts;
        state_.players = std::move(players);
        for (PlayerState& p : state_.players) {
            p.score   = opts.startingScore;
            p.legsWon = 0;
            p.throws.clear();
        }
        state_.currentIndex = 0;
        state_.dartIndex    = 0;
        state_.turns        = {{}};
        state_.winner.reset();
        state_.finishedPlayers.clear();
        state_.gameOver = false;
        history_.clear();
        has_game_ = true;
        copy = state_;
    }
    if (on_changed_) on_changed_(copy);  // outside the lock (callbacks may re-enter)
}

bool GameManager::hasGame() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return has_game_;
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
    ApplyResult res = applyThrow(state_, thr);
    GameState ns = std::move(res.state);

    if (res.finished && !contains(ns.finishedPlayers, playerId)) {
        ns.finishedPlayers.push_back(playerId);

        const PlayerState* fp = findById(ns, playerId);
        if (!ns.winner.has_value() && fp && fp->legsWon >= ns.options.legs)
            ns.winner = playerId;

        const std::vector<int> remaining = activePlayerIds(ns);
        if (remaining.size() <= 1) {
            ns.gameOver = true;
            if (remaining.size() == 1 && !contains(ns.finishedPlayers, remaining[0]))
                ns.finishedPlayers.push_back(remaining[0]);
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
        GameState rec = recalculateFromTurns(state_);

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
    const PlayerState& p = state_.players[state_.currentIndex];
    const int dartsLeft = 3 - state_.dartIndex;
    return getCheckoutSuggestion(p.score, dartsLeft, state_.options.outType);
}

void GameManager::notify_(const GameState& s) {
    if (on_changed_) on_changed_(s);
}

} // namespace dart::game
