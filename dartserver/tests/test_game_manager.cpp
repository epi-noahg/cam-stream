// Tests for GameManager match-level bookkeeping: winner assignment, gameOver,
// correction round-trip, undo.

#include "game/GameManager.hpp"

#include <iostream>
#include <string>

using namespace dart::game;

static int g_failures = 0;
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ++g_failures;                                                      \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  "        \
                      << #cond << "\n";                                        \
        }                                                                      \
    } while (0)

namespace {

Throw T(int v, int m = 1) { return Throw{v, m, false}; }

// GameManager holds a mutex (non-movable), so configure one in place.
void setup(GameManager& m, int startingScore, int nplayers = 2) {
    OptionsX01 opts;
    opts.startingScore = startingScore;
    opts.outType = InOutType::Double;
    opts.legs = 1;
    std::vector<PlayerState> players;
    for (int i = 0; i < nplayers; ++i)
        players.push_back(PlayerState{i, "P" + std::to_string(i), startingScore, 0, {}});
    m.createGame(std::move(players), opts);
}

void testWinnerAndGameOverTwoPlayers() {
    GameManager m; setup(m, 40);
    int changes = 0;
    m.setOnChanged([&](const GameState&) { ++changes; });

    m.recordManualThrow(T(20, 2));   // P0 hits D20 → finishes, 2 players → gameOver
    GameState s = m.snapshot();
    CHECK(s.players[0].score == 0);
    CHECK(s.players[0].legsWon == 1);
    CHECK(s.winner.has_value() && s.winner.value() == 0);
    CHECK(s.gameOver);
    CHECK(changes >= 1);
}

void testCorrectionRecomputes() {
    GameManager m; setup(m, 501);
    m.recordManualThrow(T(20, 3));   // 60
    m.recordManualThrow(T(20, 3));   // 60
    m.recordManualThrow(T(20, 3));   // 60 → end turn
    GameState before = m.snapshot();
    CHECK(before.players[0].score == 321);

    // The first dart was actually a single 20, not a triple.
    m.correctThrow(0, 0, T(20, 1));
    GameState after = m.snapshot();
    CHECK(after.players[0].score == 501 - 20 - 60 - 60);   // 361
}

void testUndo() {
    GameManager m; setup(m, 501);
    m.recordManualThrow(T(20, 3));   // 441
    CHECK(m.snapshot().players[0].score == 441);
    m.recordManualThrow(T(19, 3));   // 384
    CHECK(m.snapshot().players[0].score == 384);
    m.undo();
    CHECK(m.snapshot().players[0].score == 441);
}

void testCheckoutExposed() {
    GameManager m; setup(m, 40);
    auto co = m.currentCheckout();
    CHECK(co.has_value());
    CHECK(co->size() == 1 && (*co)[0].multiplier == 2 && (*co)[0].value == 20);
}

// Teams P0,P2 = team 1 ; P1,P3 = team 2 — share score within a team.
void setupTeams(GameManager& m, int startingScore) {
    OptionsX01 opts;
    opts.startingScore = startingScore;
    opts.outType = InOutType::Double;
    opts.legs = 1;
    opts.teams = 2;
    std::vector<PlayerState> players = {
        {0, "A", startingScore, 0, 1, {}},
        {1, "B", startingScore, 0, 2, {}},
        {2, "C", startingScore, 0, 1, {}},
        {3, "D", startingScore, 0, 2, {}},
    };
    m.createGame(std::move(players), opts);
}

void testTeamSharedScore() {
    GameManager m; setupTeams(m, 501);
    m.recordManualThrow(T(20, 3));   // P0 (team1): -60 -> 441
    GameState s = m.snapshot();
    CHECK(s.players[0].score == 441);
    CHECK(s.players[2].score == 441);   // teammate shares the team score
    CHECK(s.players[1].score == 501);   // other team untouched
    CHECK(s.players[3].score == 501);
    CHECK(s.currentIndex == 0);         // still P0's turn (dart 2 of 3)
}

void testTeamFinish() {
    GameManager m; setupTeams(m, 40);
    m.recordManualThrow(T(20, 2));   // P0 D20 = 40 -> team1 finishes
    GameState s = m.snapshot();
    CHECK(s.players[0].score == 0);
    CHECK(s.players[2].score == 0);     // teammate shares
    CHECK(s.gameOver);                  // only team2 remains
    CHECK(s.winner.has_value() && s.winner.value() == 0);
    CHECK(s.finishedPlayers.size() == 4);
}

} // namespace

int main() {
    testWinnerAndGameOverTwoPlayers();
    testCorrectionRecomputes();
    testUndo();
    testCheckoutExposed();
    testTeamSharedScore();
    testTeamFinish();

    if (g_failures == 0) std::cout << "All GameManager tests passed.\n";
    else std::cerr << g_failures << " check(s) failed.\n";
    return g_failures;
}
