// Unit tests for the pure Round the Clock engine.
// Same minimal harness as test_x01.cpp.

#include "game/RoundClockEngine.hpp"

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

GameState makeRTC(int nplayers = 2) {
    GameState s;
    s.mode = GameMode::RoundTheClock;
    for (int i = 0; i < nplayers; ++i) {
        PlayerState p;
        p.id       = i;
        p.nickname = "P" + std::to_string(i);
        p.target   = 1;
        s.players.push_back(std::move(p));
    }
    return s;
}

Throw T(int v, int m = 1) { return Throw{v, m, false}; }

ApplyResult hit(GameState& s, int playerIndex, const Throw& t) {
    s.currentIndex = playerIndex;
    ApplyResult r = applyRoundClockThrow(s, t);
    s = r.state;
    return r;
}

void testAdvanceOnHit() {
    GameState s = makeRTC();
    hit(s, 0, T(1));                     // hits target 1 → advance to 2
    CHECK(s.players[0].target == 2);
    hit(s, 0, T(2));                     // hits target 2 → advance to 3
    CHECK(s.players[0].target == 3);
}

void testMissDoesNotAdvance() {
    GameState s = makeRTC();
    hit(s, 0, T(5));                     // target is 1, hit 5 → no advance
    CHECK(s.players[0].target == 1);
}

void testAnyMultiplierAdvancesByOne() {
    GameState s = makeRTC();
    hit(s, 0, T(1, 3));                  // triple 1 still advances only by 1
    CHECK(s.players[0].target == 2);
    hit(s, 0, T(2, 2));                  // double 2 advances by 1
    CHECK(s.players[0].target == 3);
}

void testWinAtTwentyOne() {
    GameState s = makeRTC(1);
    ApplyResult r{};
    for (int n = 1; n <= 20; ++n) {
        r = hit(s, 0, T(n));
        if (n < 20) CHECK(!r.finished);
    }
    CHECK(r.finished);
    CHECK(s.players[0].target == 21);
    CHECK(s.players[0].legsWon == 1);
}

void testRecalculate() {
    GameState s = makeRTC(2);
    s.turns = {
        { T(1), T(2), T(9) },   // turn 0 → p0: hits 1, hits 2, misses → target 3
        { T(1) },               // turn 1 → p1: hits 1 → target 2
    };
    GameState rec = recalculateRoundClock(s);
    CHECK(rec.players[0].target == 3);
    CHECK(rec.players[1].target == 2);
}

} // namespace

int main() {
    testAdvanceOnHit();
    testMissDoesNotAdvance();
    testAnyMultiplierAdvancesByOne();
    testWinAtTwentyOne();
    testRecalculate();

    if (g_failures == 0) std::cout << "All Round the Clock engine tests passed.\n";
    else std::cerr << g_failures << " check(s) failed.\n";
    return g_failures;
}
