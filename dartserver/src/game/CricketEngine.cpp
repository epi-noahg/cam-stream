#include "CricketEngine.hpp"

#include <algorithm>

namespace dart::game {
namespace {

constexpr int kMarksToClose = 3;

/// Team identity: solo players (team 0) are their own team.  Mirrors the helper
/// in GameManager so team-mates share marks and points.
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

/// Index into kCricketTargets for this throw, or -1 if it is not a target.
int targetIndexFor(const Throw& thr, bool useBull) {
    const int v = thr.value;
    if (v == 25 || v == 50) return useBull ? 6 : -1;
    for (int i = 0; i < 6; ++i) if (kCricketTargets[i] == v) return i;
    return -1;
}

/// Marks a throw is worth: the multiplier for 15..20; the outer bull (25) is 1
/// and the inner bull (50) is 2.
int marksForThrow(const Throw& thr) {
    if (thr.value == 50) return 2;
    if (thr.value == 25) return 1;
    return thr.multiplier;
}

void ensureMarks(PlayerState& p) {
    if (p.marks.size() != 7) p.marks.assign(7, 0);
}

/// Copy a player's shared Cricket state (marks + score + legsWon) onto every
/// team-mate.  No-op for solo players (team == 0).
void propagateTeam(GameState& ns, int id) {
    PlayerState* a = nullptr;
    for (PlayerState& p : ns.players) if (p.id == id) { a = &p; break; }
    if (!a || a->team == 0) return;
    for (PlayerState& p : ns.players)
        if (p.id != id && p.team == a->team) {
            p.marks   = a->marks;
            p.score   = a->score;
            p.legsWon = a->legsWon;
        }
}

/// Whether @p p has closed every required target (bull included only if used).
bool hasClosedAll(const PlayerState& p, bool useBull) {
    if (p.marks.size() != 7) return false;
    const int last = useBull ? 7 : 6;
    for (int i = 0; i < last; ++i) if (p.marks[i] < kMarksToClose) return false;
    return true;
}

/// Win check after the acting player closed everything: standard cricket needs
/// the lead on points, cut-throat needs the fewest points.
bool meetsPointGoal(const GameState& s, const PlayerState& actor) {
    const bool cut = s.cricket.cutThroat;
    bool sawOpponent = false;
    for (const PlayerState& p : s.players) {
        if (teamKey(p) == teamKey(actor)) continue;
        sawOpponent = true;
        if (cut) { if (actor.score > p.score) return false; }   // must not trail
        else     { if (actor.score < p.score) return false; }   // must lead/tie
    }
    (void)sawOpponent;  // solo (no opponents) → closing all is enough
    return true;
}

} // namespace

ApplyResult applyCricketThrow(const GameState& state, const Throw& thr) {
    const PlayerState& player = state.players[state.currentIndex];
    const int actingId = player.id;

    auto ret = [&](GameState ns, bool finished) -> ApplyResult {
        propagateTeam(ns, actingId);
        return {std::move(ns), /*bust=*/false, finished};
    };

    // Player already finished → ignore the throw entirely.
    if (playerFinished(state, actingId)) return ret(state, false);

    GameState ns = state;
    PlayerState& me = ns.players[state.currentIndex];
    ensureMarks(me);
    me.throws.push_back(thr);

    const int ti = targetIndexFor(thr, ns.cricket.useBull);
    if (ti >= 0) {
        const int value        = kCricketTargets[ti];
        const int marks        = marksForThrow(thr);
        const int closing      = std::min(marks, std::max(0, kMarksToClose - me.marks[ti]));
        const int scoringMarks = marks - closing;
        me.marks[ti] += closing;

        if (scoringMarks > 0) {
            if (ns.cricket.cutThroat) {
                // Dump points on every opponent TEAM that has not closed this
                // number — once per team, not per member (team-mates share the
                // score via propagateTeam, so charging each member would
                // double-count for multi-player teams).
                std::vector<int> chargedTeams;
                for (PlayerState& op : ns.players) {
                    const int tk = teamKey(op);
                    if (tk == teamKey(me)) continue;
                    bool already = false;
                    for (int c : chargedTeams) if (c == tk) { already = true; break; }
                    if (already) continue;
                    ensureMarks(op);
                    if (op.marks[ti] < kMarksToClose) {
                        op.score += scoringMarks * value;
                        propagateTeam(ns, op.id);
                        chargedTeams.push_back(tk);
                    }
                }
            } else {
                // Standard: bank points while any opponent still has it open.
                bool anyOpen = false;
                for (const PlayerState& op : ns.players) {
                    if (teamKey(op) == teamKey(me)) continue;
                    if (op.marks.size() != 7 || op.marks[ti] < kMarksToClose)
                        anyOpen = true;
                }
                if (anyOpen) me.score += scoringMarks * value;
            }
        }
    }

    // Win check: all targets closed and the point goal met.
    const bool finished =
        hasClosedAll(me, ns.cricket.useBull) && meetsPointGoal(ns, me);
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

    return ret(std::move(ns), finished);
}

GameState recalculateCricket(const GameState& state) {
    // Authoritative turn history.  applyCricketThrow() mutates a scratch copy's
    // `turns` while replaying, so keep the original and restore it at the end.
    const std::vector<std::vector<Throw>> recorded = state.turns;
    const int nplayers = static_cast<int>(state.players.size());

    GameState cur = state;
    for (PlayerState& p : cur.players) {
        p.score   = 0;
        p.legsWon = 0;
        p.marks.assign(7, 0);
        p.throws.clear();
    }
    cur.currentIndex = 0;
    cur.dartIndex    = 0;
    cur.winner       = std::nullopt;
    cur.finishedPlayers.clear();
    cur.gameOver     = false;

    // Replay every recorded dart.  Turn `i` belongs to player `i % nplayers`;
    // the manager preserves that alignment even across manual hand-overs
    // (nextPlayer materialises a turn slot per player it steps over).  Cricket
    // ends on the first finish, so we stop replaying there.
    bool over        = false;
    int  winnerIndex = -1;
    for (std::size_t turnIndex = 0; turnIndex < recorded.size() && !over; ++turnIndex) {
        const std::vector<Throw>& turn = recorded[turnIndex];
        const int playerIndex = static_cast<int>(turnIndex) % nplayers;
        for (std::size_t throwIndex = 0; throwIndex < turn.size(); ++throwIndex) {
            cur.currentIndex = playerIndex;
            cur.dartIndex    = static_cast<int>(throwIndex);
            ApplyResult r = applyCricketThrow(cur, turn[throwIndex]);
            cur = std::move(r.state);
            if (r.finished) { over = true; winnerIndex = playerIndex; break; }
        }
    }

    cur.turns = recorded;  // discard the replay's scratch turn mutations

    if (over) {
        // Mirror GameManager's Cricket finish bookkeeping: the winner's whole
        // team finishes, every remaining player is placed, and the match ends.
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
        // Resume on whoever owns the last (in-progress) turn.
        const int lastTurn = static_cast<int>(recorded.size()) - 1;
        cur.currentIndex = lastTurn % nplayers;
        cur.dartIndex    = static_cast<int>(recorded[lastTurn].size()) % 3;
    }
    return cur;
}

} // namespace dart::game
