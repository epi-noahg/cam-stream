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

} // namespace

int main() {
    testWinnerAndGameOverTwoPlayers();
    testCorrectionRecomputes();
    testUndo();
    testCheckoutExposed();

    if (g_failures == 0) std::cout << "All GameManager tests passed.\n";
    else std::cerr << g_failures << " check(s) failed.\n";
    return g_failures;
}
