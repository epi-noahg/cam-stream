// Unit tests for the pure X01 engine + checkout solver.
// Minimal harness: each CHECK records a failure; main returns the count so
// CTest treats a non-zero exit as a failed test.

#include "game/Checkout.hpp"
#include "game/X01Engine.hpp"

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

GameState makeGame(int startingScore = 501,
                   InOutType outType = InOutType::Double,
                   InOutType inType  = InOutType::Any,
                   int nplayers = 2) {
    GameState s;
    s.options.startingScore = startingScore;
    s.options.outType       = outType;
    s.options.inType        = inType;
    s.options.legs          = 1;
    s.options.allowBust     = false;
    for (int i = 0; i < nplayers; ++i)
        s.players.push_back(PlayerState{i, "P" + std::to_string(i),
                                        startingScore, 0, {}});
    return s;
}

Throw T(int v, int m = 1) { return Throw{v, m, false}; }

void testBasicScoring() {
    GameState s = makeGame();
    s = applyThrow(s, T(20, 3)).state;   // T20 = 60
    CHECK(s.players[0].score == 441);
    CHECK(s.currentIndex == 0);
    CHECK(s.dartIndex == 1);
    s = applyThrow(s, T(20, 3)).state;   // 381
    s = applyThrow(s, T(20, 3)).state;   // 321, end of turn → player 1
    CHECK(s.players[0].score == 321);
    CHECK(s.currentIndex == 1);
    CHECK(s.dartIndex == 0);
    CHECK(s.turns.size() == 2);          // closed turn + fresh empty turn
    CHECK(s.turns[0].size() == 3);
    CHECK(s.turns[1].empty());
}

void testBullsIgnoreMultiplier() {
    GameState s = makeGame();
    s = applyThrow(s, T(50)).state;      // inner bull = 50
    CHECK(s.players[0].score == 451);
    s = applyThrow(s, T(25)).state;      // outer bull = 25
    CHECK(s.players[0].score == 426);
}

void testBustBelowZero() {
    GameState s = makeGame(50);
    auto r = applyThrow(s, T(20, 3));     // 60 > 50 → bust
    CHECK(r.bust);
    CHECK(r.state.players[0].score == 50);   // restored
    CHECK(r.state.currentIndex == 1);        // turn handed over
    CHECK(r.state.players[0].throws.back().bust);
}

void testBustOnOne() {
    GameState s = makeGame(51);
    auto r = applyThrow(s, T(50));        // leaves 1 → bust
    CHECK(r.bust);
    CHECK(r.state.players[0].score == 51);
}

void testDoubleOutBustsOnSingle() {
    GameState s = makeGame(20);
    auto r = applyThrow(s, T(20, 1));     // reaches 0 on a single → bust (double out)
    CHECK(r.bust);
    CHECK(!r.finished);
    CHECK(r.state.players[0].score == 20);
}

void testDoubleOutFinishesAndWins() {
    // applyThrow stays pure: it reports `finished` and awards the leg, but
    // setting GameState.winner is GameManager's job (front-end parity, where
    // the reducer — not applyThrow — assigns the winner).
    GameState s = makeGame(40);
    auto r = applyThrow(s, T(20, 2));     // D20 = 40, valid double out
    CHECK(r.finished);
    CHECK(!r.bust);
    CHECK(r.state.players[0].score == 0);
    CHECK(r.state.players[0].legsWon == 1);
}

void testAnyOutFinishesOnSingle() {
    GameState s = makeGame(20, InOutType::Any);
    auto r = applyThrow(s, T(20, 1));
    CHECK(r.finished);
    CHECK(r.state.players[0].score == 0);
}

void testDoubleInDoesNotOpenOnSingle() {
    GameState s = makeGame(501, InOutType::Double, InOutType::Double);
    auto r = applyThrow(s, T(20, 1));     // single does not open
    CHECK(!r.bust);
    CHECK(r.state.players[0].score == 501);   // no score change
    CHECK(r.state.players[0].throws.size() == 1);
    auto r2 = applyThrow(r.state, T(20, 2)); // double opens
    CHECK(r2.state.players[0].score == 461);
}

void testRecalculateAfterEdit() {
    GameState s = makeGame();
    // Player 0 throws a full turn 60/60/60, player 1 a full turn.
    s = applyThrow(s, T(20, 3)).state;
    s = applyThrow(s, T(20, 3)).state;
    s = applyThrow(s, T(20, 3)).state;    // p0: 321, now p1
    CHECK(s.players[0].score == 321);

    // Correct the first throw to a single 20 (20 instead of 60).
    s.turns[0][0] = T(20, 1);
    GameState rec = recalculateFromTurns(s);
    CHECK(rec.players[0].score == 501 - 20 - 60 - 60);  // 361
}

void testCheckoutSolver() {
    auto c40 = getCheckoutSuggestion(40, 3, InOutType::Double);
    CHECK(c40.has_value());
    CHECK(c40->size() == 1);
    CHECK((*c40)[0].value == 20 && (*c40)[0].multiplier == 2);

    auto c170 = getCheckoutSuggestion(170, 3, InOutType::Double);
    CHECK(c170.has_value());
    CHECK(c170->size() == 3);

    CHECK(canCheckout(170, InOutType::Double));
    CHECK(!canCheckout(169, InOutType::Double));   // classic impossible
    CHECK(!canCheckout(1, InOutType::Double));
    CHECK(getCheckoutSuggestion(171, 3, InOutType::Double) == std::nullopt);
}

} // namespace

int main() {
    testBasicScoring();
    testBullsIgnoreMultiplier();
    testBustBelowZero();
    testBustOnOne();
    testDoubleOutBustsOnSingle();
    testDoubleOutFinishesAndWins();
    testAnyOutFinishesOnSingle();
    testDoubleInDoesNotOpenOnSingle();
    testRecalculateAfterEdit();
    testCheckoutSolver();

    if (g_failures == 0) std::cout << "All X01 engine tests passed.\n";
    else std::cerr << g_failures << " check(s) failed.\n";
    return g_failures;
}
