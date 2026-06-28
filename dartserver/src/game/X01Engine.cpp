#include "X01Engine.hpp"

namespace dart::game {
namespace {

/// True if the throw satisfies an in/out rule.  Inner bull (50) counts as a
/// double for the "Double" requirement, matching the front-end rule.
bool matchesMultiplier(const Throw& thr, InOutType req) {
    switch (req) {
        case InOutType::Any:    return true;
        case InOutType::Double: return thr.multiplier == 2 || thr.value == 50;
        case InOutType::Triple: return thr.multiplier == 3;
    }
    return true;
}

/// Classic X01 bust test: going below zero, landing on 1, or reaching 0
/// without satisfying the out rule (unless busts are disabled).
bool isBust(int currentScore, int hitValue, const Throw& thr,
            const OptionsX01& opts) {
    const int newScore = currentScore - hitValue;
    if (newScore < 0 || newScore == 1) return true;
    if (newScore == 0 && !matchesMultiplier(thr, opts.outType) && !opts.allowBust)
        return true;
    return false;
}

/// Index of the next player (starting at @p startIndex) who has not finished.
/// Falls back to the current player if everyone is done.
int findNextActivePlayer(const GameState& s, int startIndex) {
    int idx = startIndex;
    for (std::size_t attempts = 0; attempts < s.players.size(); ++attempts) {
        const int id = s.players[idx].id;
        bool finished = false;
        for (int f : s.finishedPlayers) if (f == id) { finished = true; break; }
        if (!finished) return idx;
        idx = (idx + 1) % static_cast<int>(s.players.size());
    }
    return s.currentIndex;
}

bool playerFinished(const GameState& s, int id) {
    for (int f : s.finishedPlayers) if (f == id) return true;
    return false;
}

/// Keep teammates of @p actingId in sync (shared team score + legsWon). No-op
/// for solo players (team == 0).
void propagateTeam(GameState& ns, int actingId) {
    const PlayerState* a = nullptr;
    for (const PlayerState& p : ns.players) if (p.id == actingId) { a = &p; break; }
    if (!a || a->team == 0) return;
    const int team = a->team, score = a->score, legs = a->legsWon;
    for (PlayerState& p : ns.players)
        if (p.id != actingId && p.team == team) { p.score = score; p.legsWon = legs; }
}

} // namespace

ApplyResult applyThrow(const GameState& state, const Throw& thr) {
    const OptionsX01& opts = state.options;
    const PlayerState& player = state.players[state.currentIndex];
    const int actingId = player.id;

    // Propagate the acting player's shared score/legs to teammates on the way
    // out, so all return paths keep a team consistent.
    auto ret = [&](GameState ns, bool bust, bool finished) -> ApplyResult {
        propagateTeam(ns, actingId);
        return {std::move(ns), bust, finished};
    };

    // Player already finished → ignore the throw entirely.
    if (playerFinished(state, player.id))
        return ret(state, false, false);

    // Throw already flagged bust → record for history, no score change.
    if (thr.bust) {
        GameState ns = state;
        ns.players[state.currentIndex].throws.push_back(thr);
        ns.dartIndex = state.dartIndex + 1;
        return ret(std::move(ns), true, false);
    }

    const int hitValue = thr.hitValue();

    // "Opened" check for double/triple-in: open if rule is Any, the player has
    // already scored, or this throw matches the in rule.
    const bool playerOpened =
        opts.inType == InOutType::Any ||
        player.score != opts.startingScore ||
        matchesMultiplier(thr, opts.inType);

    if (!playerOpened) {
        GameState ns = state;
        ns.players[state.currentIndex].throws.push_back(thr);
        ns.dartIndex = state.dartIndex + 1;
        return ret(std::move(ns), false, false);
    }

    const bool bust = !opts.allowBust && isBust(player.score, hitValue, thr, opts);

    if (bust) {
        // Restore the score to the start of the current turn, flag the throw,
        // append it to the (closed) turn, and open a fresh turn for the next.
        const std::vector<Throw>& currentTurn =
            state.turns.empty() ? std::vector<Throw>{} : state.turns.back();
        int scoreAtStartOfTurn = player.score;
        for (const Throw& t : currentTurn)
            if (!t.bust) scoreAtStartOfTurn += t.hitValue();

        Throw busted = thr;
        busted.bust = true;

        GameState ns = state;
        ns.players[state.currentIndex].score = scoreAtStartOfTurn;
        ns.players[state.currentIndex].throws.push_back(busted);

        if (!ns.turns.empty()) ns.turns.back().push_back(busted);
        else                   ns.turns.push_back({busted});
        ns.turns.push_back({});  // empty turn for next player

        ns.currentIndex = findNextActivePlayer(
            state, (state.currentIndex + 1) % static_cast<int>(state.players.size()));
        ns.dartIndex = 0;
        return ret(std::move(ns), true, false);
    }

    // Normal scoring.
    const int newScore = player.score - hitValue;
    const bool finished = newScore == 0 && matchesMultiplier(thr, opts.outType);

    GameState ns = state;
    const bool shouldWinLeg = finished && !state.winner.has_value();
    {
        PlayerState& p = ns.players[state.currentIndex];
        p.score = newScore;
        p.throws.push_back(thr);
        if (shouldWinLeg) p.legsWon += 1;
    }

    ns.dartIndex = (state.dartIndex + 1) % 3;

    bool gameWon = false;
    if (shouldWinLeg)
        for (const PlayerState& p : ns.players)
            if (p.legsWon >= opts.legs) { gameWon = true; break; }

    const bool endOfTurn = ns.dartIndex == 0 || finished || gameWon;

    if (endOfTurn) {
        if (!ns.turns.empty()) ns.turns.back().push_back(thr);
        else                   ns.turns.push_back({thr});
        if (!finished && !gameWon) ns.turns.push_back({});

        ns.currentIndex = (finished || gameWon)
            ? state.currentIndex
            : findNextActivePlayer(
                  state, (state.currentIndex + 1) % static_cast<int>(state.players.size()));
        ns.dartIndex = 0;
    } else {
        if (!ns.turns.empty()) ns.turns.back().push_back(thr);
        else                   ns.turns.push_back({thr});
    }

    return ret(std::move(ns), false, finished);
}

GameState recalculateFromTurns(const GameState& state) {
    const int nplayers = static_cast<int>(state.players.size());

    // Start from a clean slate: keep the players/options but reset all per-turn
    // progress.  Crucially `turns` is rebuilt from scratch (below) instead of
    // letting applyThrow append onto the existing list — doing the latter
    // doubled the history on every correction and corrupted later replays.
    GameState cur = state;
    for (PlayerState& p : cur.players) {
        p.score   = state.options.startingScore;
        p.legsWon = 0;
        p.throws.clear();
    }
    cur.currentIndex = 0;
    cur.dartIndex    = 0;
    cur.winner       = std::nullopt;
    cur.finishedPlayers.clear();
    cur.gameOver     = false;

    std::vector<std::vector<Throw>> rebuilt;
    rebuilt.reserve(state.turns.size());

    for (std::size_t turnIndex = 0; turnIndex < state.turns.size(); ++turnIndex) {
        const std::vector<Throw>& turn = state.turns[turnIndex];
        const int playerIndex =
            nplayers > 0
                ? static_cast<int>(turnIndex % static_cast<std::size_t>(nplayers))
                : 0;

        // Hand the engine a single fresh in-progress turn to fill, so its
        // bust-restoration sums only this turn and nothing leaks across turns.
        cur.turns = {{}};

        for (std::size_t throwIndex = 0; throwIndex < turn.size(); ++throwIndex) {
            cur.currentIndex = playerIndex;
            cur.dartIndex    = static_cast<int>(throwIndex);

            // Drop the stored bust flag and let the engine recompute it against
            // the (possibly edited) value — a correction can clear a stale bust
            // or introduce a new one.
            Throw t = turn[throwIndex];
            t.bust = false;

            ApplyResult r = applyThrow(cur, t);
            cur = std::move(r.state);

            if (r.finished) {
                const int id = cur.players[playerIndex].id;
                bool seen = false;
                for (int x : cur.finishedPlayers) if (x == id) { seen = true; break; }
                if (!seen) cur.finishedPlayers.push_back(id);
                // First player to finish wins the leg; recording the winner here
                // stops applyThrow awarding a leg to later finishers (placings).
                if (!cur.winner.has_value()) cur.winner = id;
            }
        }

        // Capture the engine's recomputed throws for this turn (its first
        // bucket) into the clean rebuilt history.
        rebuilt.push_back(cur.turns.empty() ? std::vector<Throw>{}
                                            : cur.turns.front());
    }

    cur.turns = std::move(rebuilt);
    return cur;
}

} // namespace dart::game
