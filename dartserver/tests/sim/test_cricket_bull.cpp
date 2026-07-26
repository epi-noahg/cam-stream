// Nasty-case simulation tests: Cricket BULL mechanics.
// Asserts AUTHORITATIVE bull rules (25=1 mark, 50=2 marks, points at 25/mark,
// useBull gating). Some CHECKs are EXPECTED to fail against today's buggy code.
#include "game/GameManager.hpp"
#include <iostream>
#include <string>
#include <vector>
using namespace dart::game;
static int g_failures = 0;
#define CHECK(cond) do { if(!(cond)){ ++g_failures; \
  std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<"  "<<#cond<<"\n"; } } while(0)
static Throw T(int v, int mult=1){ return Throw{v, mult, false}; }
// Build+start a Cricket game in-place. teamOf: optional team id per player (0=solo).
static void newCricket(GameManager& m, int nplayers, bool cutThroat=false,
                       bool useBull=true, std::vector<int> teamOf={}) {
    std::vector<PlayerState> ps;
    for (int i=0;i<nplayers;i++){ PlayerState p; p.id=i; p.nickname="P"+std::to_string(i);
        p.team = (i<(int)teamOf.size()?teamOf[i]:0); ps.push_back(p);}
    GameConfig cfg; cfg.mode=GameMode::Cricket;
    cfg.cricket.cutThroat=cutThroat; cfg.cricket.useBull=useBull;
    cfg.cricket.teams = teamOf.empty()?1:2;
    m.createGame(std::move(ps), cfg);
}

// marks index: [20,19,18,17,16,15,25]; bull is index 6.
static const int BULL = 6;

// ── 1. inner bull (50) = 2 marks on marks[6] ────────────────────────────────
static void innerBullTwoMarks() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(50));
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[BULL] == 2);
    CHECK(gs.players[0].score == 0);        // 2 < 3, not closed, no overflow
    CHECK(!gs.gameOver);
}

// ── 2. outer bull (25) = 1 mark ─────────────────────────────────────────────
static void outerBullOneMark() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(25));
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[BULL] == 1);
    CHECK(gs.players[0].score == 0);
}

// ── 3. inner then outer closes bull (2+1=3), no overflow ────────────────────
static void innerThenOuterCloses() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(50));
    m.recordManualThrow(T(25));
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[BULL] == 3);
    CHECK(gs.players[0].score == 0);        // exactly closed, zero overflow
}

// ── 4. two outer bulls = 2 marks ────────────────────────────────────────────
static void twoOuterBulls() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(25));
    m.recordManualThrow(T(25));
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[BULL] == 2);
    CHECK(gs.players[0].score == 0);
}

// ── 5. 50+50 = 4 marks: close (3) + 1 overflow; standard banks 25 (opp open) ─
static void doubleInnerCloseWithOverflowStandard() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(50));
    m.recordManualThrow(T(50));
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[BULL] == 3);
    CHECK(gs.players[0].score == 25);       // 1 overflow mark * 25, opponent open
    CHECK(gs.players[1].score == 0);        // standard: opponent untouched
}

// ── 6. overflow banks 25/mark while opponent bull open ──────────────────────
static void overflowBanksWhileOpponentOpen() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(50));             // 2 marks
    m.recordManualThrow(T(25));             // closes (3)
    m.recordManualThrow(T(25));             // 1 overflow -> +25
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[BULL] == 3);
    CHECK(gs.players[0].score == 25);
    CHECK(gs.players[1].score == 0);
}

// ── 7. once BOTH have closed bull, further overflow scores nothing ──────────
static void overflowNoPointsWhenOpponentClosed() {
    GameManager m; newCricket(m, 2);
    // P0 visit: close + 1 overflow while P1 open -> banks 25.
    m.recordManualThrow(T(50));
    m.recordManualThrow(T(50));
    m.nextPlayer();                         // hand to P1
    // P1 visit: close bull; P0 already closed -> P1 banks nothing.
    m.recordManualThrow(T(50));
    m.recordManualThrow(T(50));
    m.nextPlayer();                         // hand to P0
    // P0 again: overflow bull, but P1 now closed -> no points.
    m.recordManualThrow(T(25));
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[BULL] == 3);
    CHECK(gs.players[1].marks[BULL] == 3);
    CHECK(gs.players[0].score == 25);       // unchanged since opponent closed
    CHECK(gs.players[1].score == 0);        // never banked (P0 was closed)
}

// ── 8. cut-throat: bull overflow dumps 25/mark onto open opponent ───────────
static void cutThroatDumpsOnOpenOpponent() {
    GameManager m; newCricket(m, 2, /*cutThroat=*/true);
    m.recordManualThrow(T(50));
    m.recordManualThrow(T(50));             // close + 1 overflow
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[BULL] == 3);
    CHECK(gs.players[0].score == 0);        // thrower gains nothing
    CHECK(gs.players[1].score == 25);       // dumped on open opponent
}

// ── 9. useBull=false: 25/50 ignored entirely (no marks, no score) ───────────
static void useBullFalseIgnoresBull() {
    GameManager m; newCricket(m, 2, /*cutThroat=*/false, /*useBull=*/false);
    m.recordManualThrow(T(50));
    m.recordManualThrow(T(25));
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[BULL] == 0);
    CHECK(gs.players[0].score == 0);
    CHECK(!gs.gameOver);
}

// ── 10. useBull=false: winning does NOT require the bull ─────────────────────
static void useBullFalseWinsWithoutBull() {
    GameManager m; newCricket(m, 2, /*cutThroat=*/false, /*useBull=*/false);
    // P0 visit1: close 20,19,18.
    m.recordManualThrow(T(20,3));
    m.recordManualThrow(T(19,3));
    m.recordManualThrow(T(18,3));           // auto hand-over after 3 darts
    // P1 visit: harmless non-targets.
    m.recordManualThrow(T(1));
    m.recordManualThrow(T(1));
    m.recordManualThrow(T(1));
    // P0 visit2: close 17,16,15 -> all required (bull not needed), tie score -> win.
    m.recordManualThrow(T(17,3));
    m.recordManualThrow(T(16,3));
    m.recordManualThrow(T(15,3));
    auto gs = m.snapshot();
    CHECK(gs.gameOver);
    CHECK(gs.winner.has_value() && *gs.winner == 0);
    CHECK(gs.players[0].marks[BULL] == 0);  // never touched bull
    CHECK(gs.finishedPlayers.size() == 2);
}

// ── 11. useBull=true: closing 20..15 without bull does NOT win ───────────────
static void useBullTrueRequiresBull() {
    GameManager m; newCricket(m, 2, /*cutThroat=*/false, /*useBull=*/true);
    m.recordManualThrow(T(20,3));
    m.recordManualThrow(T(19,3));
    m.recordManualThrow(T(18,3));
    m.recordManualThrow(T(1));              // P1 filler
    m.recordManualThrow(T(1));
    m.recordManualThrow(T(1));
    m.recordManualThrow(T(17,3));
    m.recordManualThrow(T(16,3));
    m.recordManualThrow(T(15,3));           // 20..15 all closed, bull still open
    auto gs = m.snapshot();
    CHECK(!gs.gameOver);                    // bull required and still open
    CHECK(!gs.winner.has_value());
    CHECK(gs.players[0].marks[BULL] == 0);
}

// ── 12. correctThrow editing a bull dart recomputes marks/points ────────────
static void correctThrowEditBullRecomputes() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(25));             // 1
    m.recordManualThrow(T(25));             // 2
    m.recordManualThrow(T(25));             // closes (3), no overflow
    {
        auto gs = m.snapshot();
        CHECK(gs.players[0].marks[BULL] == 3);
        CHECK(gs.players[0].score == 0);
    }
    // Locate P0's first visit, first dart, upgrade 25 -> 50.
    auto turns = m.snapshot().turns;
    CHECK(!turns.empty() && turns[0].size() == 3);
    m.correctThrow(0, 0, T(50));            // now 50+25+25 = 4 marks
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[BULL] == 3);
    CHECK(gs.players[0].score == 25);       // 1 overflow * 25, opponent open
}

// ── 13. undo a bull close reverts marks[6] ──────────────────────────────────
static void undoBullCloseReverts() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(50));             // 2 marks
    {
        auto gs = m.snapshot();
        CHECK(gs.players[0].marks[BULL] == 2);
    }
    m.recordManualThrow(T(25));             // closes (3)
    {
        auto gs = m.snapshot();
        CHECK(gs.players[0].marks[BULL] == 3);
    }
    m.undo();                               // revert the close
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[BULL] == 2);
    CHECK(gs.players[0].score == 0);
}

// ── 14. bull marks ignore the multiplier (2/1 regardless) ───────────────────
static void bullMultiplierIgnored() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(Throw{25, 3, false});  // outer bull, bogus mult
    m.recordManualThrow(Throw{50, 2, false});  // inner bull, bogus mult
    auto gs = m.snapshot();
    // 25 -> 1 mark, 50 -> 2 marks; total 3 marks closes with 0 overflow.
    CHECK(gs.players[0].marks[BULL] == 3);
    CHECK(gs.players[0].score == 0);
}

int main(){
    innerBullTwoMarks();
    outerBullOneMark();
    innerThenOuterCloses();
    twoOuterBulls();
    doubleInnerCloseWithOverflowStandard();
    overflowBanksWhileOpponentOpen();
    overflowNoPointsWhenOpponentClosed();
    cutThroatDumpsOnOpenOpponent();
    useBullFalseIgnoresBull();
    useBullFalseWinsWithoutBull();
    useBullTrueRequiresBull();
    correctThrowEditBullRecomputes();
    undoBullCloseReverts();
    bullMultiplierIgnored();
    if(!g_failures) std::cout<<"all cricket_bull cases passed\n";
    else std::cerr<<g_failures<<" check(s) failed\n";
    return g_failures;
}
