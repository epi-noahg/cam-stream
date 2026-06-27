// Unit tests for the pure Cricket engine.
// Same minimal harness as test_x01.cpp: each CHECK records a failure and main
// returns the count so CTest treats a non-zero exit as a failed test.

#include "game/CricketEngine.hpp"

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

GameState makeCricket(int nplayers = 2, bool cutThroat = false) {
    GameState s;
    s.mode = GameMode::Cricket;
    s.cricket.cutThroat = cutThroat;
    s.cricket.useBull   = true;
    for (int i = 0; i < nplayers; ++i) {
        PlayerState p;
        p.id       = i;
        p.nickname = "P" + std::to_string(i);
        p.score    = 0;
        p.marks.assign(7, 0);
        s.players.push_back(std::move(p));
    }
    return s;
}

Throw T(int v, int m = 1) { return Throw{v, m, false}; }

// Apply a throw on behalf of a specific player, ignoring turn rotation so a
// single player's mechanics can be exercised in isolation.
ApplyResult hit(GameState& s, int playerIndex, const Throw& t) {
    s.currentIndex = playerIndex;
    ApplyResult r = applyCricketThrow(s, t);
    s = r.state;
    return r;
}

void testCloseWithTriple() {
    GameState s = makeCricket();
    hit(s, 0, T(20, 3));                 // triple 20 closes it, no overflow
    CHECK(s.players[0].marks[0] == 3);
    CHECK(s.players[0].score == 0);      // nothing to bank yet
}

void testMarksAccumulate() {
    GameState s = makeCricket();
    hit(s, 0, T(20, 1));                 // 1 mark
    hit(s, 0, T(20, 1));                 // 2 marks
    CHECK(s.players[0].marks[0] == 2);
    CHECK(s.players[0].score == 0);
}

void testBullMarks() {
    GameState s = makeCricket();
    hit(s, 0, T(50));                    // inner bull = 2 marks
    CHECK(s.players[0].marks[6] == 2);
    hit(s, 0, T(25));                    // outer bull = 1 mark → closed
    CHECK(s.players[0].marks[6] == 3);
}

void testNonTargetIgnored() {
    GameState s = makeCricket();
    hit(s, 0, T(7, 3));                  // 7 is not a Cricket target
    for (int i = 0; i < 7; ++i) CHECK(s.players[0].marks[i] == 0);
    CHECK(s.players[0].throws.size() == 1);  // still recorded for history
}

void testStandardScoring() {
    GameState s = makeCricket(2, /*cutThroat=*/false);
    hit(s, 0, T(20, 3));                 // p0 closes 20
    hit(s, 0, T(20, 1));                 // p1 still open → p0 banks 20
    CHECK(s.players[0].score == 20);
    CHECK(s.players[1].score == 0);
}

void testStandardNoScoreWhenAllClosed() {
    GameState s = makeCricket(2, /*cutThroat=*/false);
    hit(s, 0, T(20, 3));                 // p0 closes 20
    hit(s, 1, T(20, 3));                 // p1 closes 20 too
    hit(s, 0, T(20, 1));                 // nobody open → no points
    CHECK(s.players[0].score == 0);
}

void testCutThroatDumpsOnOpponent() {
    GameState s = makeCricket(2, /*cutThroat=*/true);
    hit(s, 0, T(20, 3));                 // p0 closes 20
    hit(s, 0, T(20, 2));                 // double 20 → 2 marks dumped on p1
    CHECK(s.players[0].score == 0);
    CHECK(s.players[1].score == 40);     // 2 * 20
}

void testCloseAndOverflowSameThrow() {
    GameState s = makeCricket(2, /*cutThroat=*/false);
    hit(s, 0, T(20, 1));                 // 1 mark
    hit(s, 0, T(20, 1));                 // 2 marks
    hit(s, 0, T(20, 3));                 // closes (1) + overflow (2) → banks 40
    CHECK(s.players[0].marks[0] == 3);
    CHECK(s.players[0].score == 40);
}

void testSoloWinClosesEverything() {
    GameState s = makeCricket(1);
    ApplyResult r{};
    for (int v : {20, 19, 18, 17, 16, 15}) r = hit(s, 0, T(v, 3));
    CHECK(!r.finished);                  // bull still open
    hit(s, 0, T(50));                    // 2 bull marks
    r = hit(s, 0, T(25));               // 3rd bull mark → all closed
    CHECK(r.finished);
    CHECK(s.players[0].legsWon == 1);
}

void testStandardTrailingDoesNotWin() {
    // p1 banks points on 19; p0 closes everything but trails → not finished.
    GameState s = makeCricket(2, /*cutThroat=*/false);
    hit(s, 1, T(19, 3));                 // p1 closes 19
    hit(s, 1, T(19, 3));                 // p1 banks 3 * 19 = 57 (p0 open)
    ApplyResult r{};
    for (int v : {20, 19, 18, 17, 16, 15}) r = hit(s, 0, T(v, 3));
    hit(s, 0, T(50));
    r = hit(s, 0, T(25));               // p0 closed all but score 0 < 57
    CHECK(!r.finished);
}

void testRecalculate() {
    GameState s = makeCricket(2);
    s.turns = {
        { T(20, 3), T(20, 1), T(20, 1) },  // turn 0 → p0: close 20, bank 40
        { T(19, 3) },                       // turn 1 → p1: close 19
    };
    GameState rec = recalculateCricket(s);
    CHECK(rec.players[0].marks[0] == 3);
    CHECK(rec.players[0].score == 40);
    CHECK(rec.players[1].marks[1] == 3);
}

} // namespace

int main() {
    testCloseWithTriple();
    testMarksAccumulate();
    testBullMarks();
    testNonTargetIgnored();
    testStandardScoring();
    testStandardNoScoreWhenAllClosed();
    testCutThroatDumpsOnOpponent();
    testCloseAndOverflowSameThrow();
    testSoloWinClosesEverything();
    testStandardTrailingDoesNotWin();
    testRecalculate();

    if (g_failures == 0) std::cout << "All Cricket engine tests passed.\n";
    else std::cerr << g_failures << " check(s) failed.\n";
    return g_failures;
}
