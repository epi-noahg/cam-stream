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

// A correction must replay cleanly: the turn history keeps its exact shape
// (it used to be doubled on every edit, which corrupted later replays).
void testCorrectionKeepsTurnHistoryClean() {
    GameManager m; setup(m, 501);
    for (int i = 0; i < 3; ++i) m.recordManualThrow(T(20, 3));  // P0 full turn
    for (int i = 0; i < 3; ++i) m.recordManualThrow(T(20, 3));  // P1 full turn
    GameState before = m.snapshot();
    const std::size_t turnsBefore = before.turns.size();       // [3,3,0]

    m.correctThrow(0, 0, T(20, 1));
    GameState after = m.snapshot();
    CHECK(after.turns.size() == turnsBefore);                  // not doubled
    CHECK(after.turns[0].size() == 3);
    CHECK(after.turns[1].size() == 3);
    CHECK(after.players[0].score == 501 - 20 - 60 - 60);       // 361
    CHECK(after.players[1].score == 321);
    CHECK(!after.gameOver);
}

// Repeated corrections must stay coherent and never end an unfinished match.
void testRepeatedCorrectionsDoNotEndGame() {
    GameManager m; setup(m, 501);
    for (int i = 0; i < 3; ++i) m.recordManualThrow(T(20, 3));  // P0: 321
    for (int i = 0; i < 3; ++i) m.recordManualThrow(T(20, 3));  // P1: 321

    m.correctThrow(0, 0, T(20, 1));   // P0 first dart 60 -> 20
    m.correctThrow(1, 0, T(20, 1));   // P1 first dart 60 -> 20
    GameState s = m.snapshot();
    CHECK(s.players[0].score == 361);
    CHECK(s.players[1].score == 361);
    CHECK(!s.gameOver);
    CHECK(!s.winner.has_value());
    CHECK(s.turns.size() == 3);
}

// A correction in a still-open solo match must not declare the game over.
void testSoloCorrectionDoesNotEndGame() {
    GameManager m; setup(m, 501, /*nplayers=*/1);
    m.recordManualThrow(T(20, 3));    // 441
    m.correctThrow(0, 0, T(19, 1));   // 482 — nowhere near a checkout
    GameState s = m.snapshot();
    CHECK(s.players[0].score == 482);
    CHECK(!s.gameOver);
    CHECK(!s.winner.has_value());
}

// Correcting a dart so it lands the checkout must finish the leg and end a
// two-player match (the inverse direction — a real finish still works).
void testCorrectionCanFinishGame() {
    GameManager m; setup(m, 40);
    m.recordManualThrow(T(20, 1));    // P0: 40 -> 20 (single, not a finish)
    GameState before = m.snapshot();
    CHECK(!before.gameOver);
    // It was actually a double 20 — a 40 checkout that wins outright.
    m.correctThrow(0, 0, T(20, 2));
    GameState s = m.snapshot();
    CHECK(s.players[0].score == 0);
    CHECK(s.gameOver);
    CHECK(s.winner.has_value() && s.winner.value() == 0);
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

// Re-choosing a mode / starting a new game must mint a fresh, unique id so the
// previous (still unfinished) match keeps its own persistence key and stays
// resumable instead of being overwritten — even for back-to-back creations
// within the same millisecond.
void testNewGameMintsFreshId() {
    GameManager m; setup(m, 501);
    const std::string id1 = m.gameId();
    CHECK(!id1.empty());
    GameState prev = m.snapshot();

    setup(m, 301);
    const std::string id2 = m.gameId();
    CHECK(!id2.empty());
    CHECK(id1 != id2);

    setup(m, 701);
    CHECK(m.gameId() != id1);
    CHECK(m.gameId() != id2);

    // Resuming a saved game restores its id so updates land on the same row.
    m.loadState(prev, id1);
    CHECK(m.gameId() == id1);
}

} // namespace

int main() {
    testWinnerAndGameOverTwoPlayers();
    testCorrectionRecomputes();
    testCorrectionKeepsTurnHistoryClean();
    testRepeatedCorrectionsDoNotEndGame();
    testSoloCorrectionDoesNotEndGame();
    testCorrectionCanFinishGame();
    testUndo();
    testCheckoutExposed();
    testTeamSharedScore();
    testTeamFinish();
    testNewGameMintsFreshId();

    if (g_failures == 0) std::cout << "All GameManager tests passed.\n";
    else std::cerr << g_failures << " check(s) failed.\n";
    return g_failures;
}
