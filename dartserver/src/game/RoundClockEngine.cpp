#include "RoundClockEngine.hpp"

namespace dart::game {
namespace {

constexpr int kLastTarget = 20;  // sequence is 1..20

int teamKey(const PlayerState& p) { return p.team == 0 ? -(p.id + 1) : p.team; }

bool playerFinished(const GameState& s, int id) {
    for (int f : s.finishedPlayers) if (f == id) return true;
    return false;
}

int findNextActivePlayer(const GameState& s, int startIndex) {
    const int n = static_cast<int>(s.players.size());
    int idx = startIndex;
    for (int attempts = 0; attempts < n; ++attempts) {
        if (!playerFinished(s, s.players[idx].id)) return idx;
        idx = (idx + 1) % n;
    }
    return s.currentIndex;
}

/// Keep team-mates in sync (shared target + legsWon).  No-op for solo players.
void propagateTeam(GameState& ns, int id) {
    PlayerState* a = nullptr;
    for (PlayerState& p : ns.players) if (p.id == id) { a = &p; break; }
    if (!a || a->team == 0) return;
    for (PlayerState& p : ns.players)
        if (p.id != id && p.team == a->team) {
            p.target  = a->target;
            p.legsWon = a->legsWon;
        }
}

} // namespace

ApplyResult applyRoundClockThrow(const GameState& state, const Throw& thr) {
    const PlayerState& player = state.players[state.currentIndex];
    const int actingId = player.id;

    auto ret = [&](GameState ns, bool finished) -> ApplyResult {
        propagateTeam(ns, actingId);
        return {std::move(ns), /*bust=*/false, finished};
    };

    if (playerFinished(state, actingId)) return ret(state, false);

    GameState ns = state;
    PlayerState& me = ns.players[state.currentIndex];
    if (me.target <= 0) me.target = 1;  // defensive: a fresh player starts at 1
    me.throws.push_back(thr);

    // Any hit on the current target advances by one.
    if (thr.value == me.target) me.target += 1;

    const bool finished = me.target > kLastTarget;
    const bool shouldWinLeg = finished && !state.winner.has_value();
    if (shouldWinLeg) me.legsWon += 1;

    ns.dartIndex = (state.dartIndex + 1) % 3;
    const bool endOfTurn = ns.dartIndex == 0 || finished;

    if (!ns.turns.empty()) ns.turns.back().push_back(thr);
    else                   ns.turns.push_back({thr});

    if (endOfTurn) {
        if (!finished) ns.turns.push_back({});
        ns.currentIndex = finished
            ? state.currentIndex
            : findNextActivePlayer(
                  state, (state.currentIndex + 1) % static_cast<int>(state.players.size()));
        ns.dartIndex = 0;
    }

    (void)teamKey;  // reserved for future team-aware scoring
    return ret(std::move(ns), finished);
}

GameState recalculateRoundClock(const GameState& state) {
    // Authoritative turn history; applyRoundClockThrow() mutates a scratch
    // copy's `turns` while replaying, so keep the original and restore it.
    const std::vector<std::vector<Throw>> recorded = state.turns;
    const int nplayers = static_cast<int>(state.players.size());

    GameState cur = state;
    for (PlayerState& p : cur.players) {
        p.target  = 1;
        p.legsWon = 0;
        p.throws.clear();
    }
    cur.currentIndex = 0;
    cur.dartIndex    = 0;
    cur.winner       = std::nullopt;
    cur.finishedPlayers.clear();
    cur.gameOver     = false;

    // Turn `i` belongs to player `i % nplayers` (alignment preserved by the
    // manager, incl. manual hand-overs).  RTC ends on the first finish.
    bool over        = false;
    int  winnerIndex = -1;
    for (std::size_t turnIndex = 0; turnIndex < recorded.size() && !over; ++turnIndex) {
        const std::vector<Throw>& turn = recorded[turnIndex];
        const int playerIndex = static_cast<int>(turnIndex) % nplayers;
        for (std::size_t throwIndex = 0; throwIndex < turn.size(); ++throwIndex) {
            cur.currentIndex = playerIndex;
            cur.dartIndex    = static_cast<int>(throwIndex);
            ApplyResult r = applyRoundClockThrow(cur, turn[throwIndex]);
            cur = std::move(r.state);
            if (r.finished) { over = true; winnerIndex = playerIndex; break; }
        }
    }

    cur.turns = recorded;  // discard the replay's scratch turn mutations

    if (over) {
        // The winner's whole team finishes; place everyone else; end the match.
        const int winnerTeam = teamKey(cur.players[winnerIndex]);
        cur.finishedPlayers.clear();
        for (const PlayerState& p : cur.players)
            if (teamKey(p) == winnerTeam) cur.finishedPlayers.push_back(p.id);
        for (const PlayerState& p : cur.players) {
            bool present = false;
            for (int f : cur.finishedPlayers) if (f == p.id) { present = true; break; }
            if (!present) cur.finishedPlayers.push_back(p.id);
        }
        cur.winner       = cur.players[winnerIndex].id;
        cur.gameOver     = true;
        cur.currentIndex = winnerIndex;
        cur.dartIndex    = 0;
    } else if (!recorded.empty()) {
        const int lastTurn = static_cast<int>(recorded.size()) - 1;
        cur.currentIndex = lastTurn % nplayers;
        cur.dartIndex    = static_cast<int>(recorded[lastTurn].size()) % 3;
    }
    return cur;
}

} // namespace dart::game
