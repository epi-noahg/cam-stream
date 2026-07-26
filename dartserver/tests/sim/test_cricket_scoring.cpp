// STANDARD Cricket (cutThroat=false) scoring & overflow — nasty-case sims.
// Asserts AUTHORITATIVE rules; failing CHECKs vs current buggy code are expected.
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

// marks index: [20,19,18,17,16,15,25(bull)]
static const PlayerState& P(const GameState& gs, int id){ return gs.players[id]; }

// ── close a number with exactly 3 marks: no overflow, score stays 0 ──────────
static void closeExact_threeSingles() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20)); m.recordManualThrow(T(20)); m.recordManualThrow(T(20));
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[0]==3);
    CHECK(P(gs,0).score==0);
    CHECK(gs.gameOver==false);
}
static void closeExact_doubleThenSingle() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20,2)); // 2 marks
    m.recordManualThrow(T(20,1)); // +1 => closed exactly
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[0]==3);
    CHECK(P(gs,0).score==0);
}
static void closeExact_singleTriple() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(19,3)); // triple => 3 marks, exact close
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[1]==3);
    CHECK(P(gs,0).score==0);
}

// ── close-and-overflow in the SAME throw (marks=2, then triple V => +2*V) ────
static void closeAndOverflow_sameThrow_20() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20,2)); // marks[0]=2
    m.recordManualThrow(T(20,3)); // 1 closes, 2 overflow => +40 (opp open)
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[0]==3);
    CHECK(P(gs,0).score==40);
}
static void closeAndOverflow_sameThrow_19() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(19,2));
    m.recordManualThrow(T(19,3)); // 1 close, 2 overflow => +38
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[1]==3);
    CHECK(P(gs,0).score==38);
}
static void closeAndOverflow_sameThrow_15() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(15,2));
    m.recordManualThrow(T(15,3)); // 1 close, 2 overflow => +30
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[5]==3);
    CHECK(P(gs,0).score==30);
}

// ── overflow banks OWN score while >=1 opponent open on V ────────────────────
static void overflow_banksWhileOpponentOpen() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20,3)); // close, no overflow
    m.recordManualThrow(T(20,3)); // 3 overflow => +60 (p1 open)
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[0]==3);
    CHECK(P(gs,0).score==60);
    CHECK(P(gs,1).score==0); // standard: opponent gains nothing
}

// ── once ALL opponents close V, further marks score nothing ──────────────────
static void overflow_noPointsWhenAllOppClosed() {
    GameManager m; newCricket(m, 2);
    // p0 closes 20
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7));
    // p1 closes 20
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7));
    // p0 throws triple 20 again — all opponents closed => no points
    m.recordManualThrow(T(20,3));
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[0]==3);
    CHECK(P(gs,1).marks[0]==3);
    CHECK(P(gs,0).score==0);
}

// ── accumulating overflow across multiple turns/numbers ──────────────────────
static void overflow_accumulateAcrossTurns() {
    GameManager m; newCricket(m, 2);
    // p0 turn: triple20 x3 => close + 6 overflow marks => +120
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(20,3)); m.recordManualThrow(T(20,3));
    // p1 harmless turn
    m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7));
    // p0 turn: triple19 x3 => close + 6 overflow => +114
    m.recordManualThrow(T(19,3)); m.recordManualThrow(T(19,3)); m.recordManualThrow(T(19,3));
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[0]==3);
    CHECK(P(gs,0).marks[1]==3);
    CHECK(P(gs,0).score==234); // 120 + 114
}

// ── non-target values (7,13,14) ignored for marks & score ────────────────────
static void nonTarget_ignored() {
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(7,3)); m.recordManualThrow(T(13,2)); m.recordManualThrow(T(14,3));
    auto gs = m.snapshot();
    for (int i=0;i<7;i++) CHECK(P(gs,0).marks[i]==0);
    CHECK(P(gs,0).score==0);
}

// ── bull close+overflow under standard (25=1 mark, 50=2; overflow val=25) ─────
static void bull_closeAndOverflow() {
    GameManager m; newCricket(m, 2, /*cutThroat*/false, /*useBull*/true);
    m.recordManualThrow(T(25)); // marks[6]=1
    m.recordManualThrow(T(25)); // marks[6]=2
    m.recordManualThrow(T(50)); // +2 => close at 3, 1 overflow => +25
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[6]==3);
    CHECK(P(gs,0).score==25);
}

// ── 3-player: overflow banks while ANY of two opponents still open ───────────
static void threePlayer_banksWhileAnyOpen() {
    GameManager m; newCricket(m, 3);
    // p0 closes 20
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7));
    // p1 closes 20
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7));
    // p2 leaves 20 open
    m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7));
    // p0 overflow triple20 => p2 still open => +60
    m.recordManualThrow(T(20,3));
    auto gs = m.snapshot();
    CHECK(P(gs,0).score==60);
    CHECK(P(gs,2).marks[0]==0);
}

// ── closed everything but TRAILING is NOT finished; then lead finishes ───────
static void closedAllButTrailing_thenLead() {
    GameManager m; newCricket(m, 2);
    // p1 first: triple20 x3 => close 20 + 6 overflow => +120 (p0 open)
    m.nextPlayer(); // hand p0's opening turn to p1 so p1 acts first
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(20,3)); m.recordManualThrow(T(20,3));
    // p0 closes 20,19,18 (exact, no overflow)
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(19,3)); m.recordManualThrow(T(18,3));
    // p1 harmless
    m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7));
    // p0 closes 17,16,15
    m.recordManualThrow(T(17,3)); m.recordManualThrow(T(16,3)); m.recordManualThrow(T(15,3));
    // p1 harmless
    m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7));
    // p0 closes bull (all targets now closed) but score 0 < 120 => NOT finished.
    // The 3rd dart (harmless) ends p0's turn cleanly.
    m.recordManualThrow(T(50)); m.recordManualThrow(T(25)); m.recordManualThrow(T(7));
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[6]==3);
    CHECK(gs.gameOver==false);
    CHECK(!gs.winner.has_value());
    // p1 harmless full turn (hands back to p0).
    m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7));
    // p0 takes the lead in a fresh turn: p1 still open on 19; each triple-19
    // banks 3*19 = 57 overflow. 57 -> 114 -> 171 (> 120) => win on the 3rd dart.
    m.recordManualThrow(T(19,3)); // 57
    m.recordManualThrow(T(19,3)); // 114
    m.recordManualThrow(T(19,3)); // 171 > 120 => win
    gs = m.snapshot();
    CHECK(P(gs,0).score==171);
    CHECK(gs.gameOver==true);
    CHECK(gs.winner.has_value() && *gs.winner==0);
    CHECK(gs.finishedPlayers.size()==2);
}

// ── exact win: solo player closing all six numbers + bull finishes ───────────
static void soloExactWin() {
    GameManager m; newCricket(m, 1);
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(19,3)); m.recordManualThrow(T(18,3));
    m.recordManualThrow(T(17,3)); m.recordManualThrow(T(16,3)); m.recordManualThrow(T(15,3));
    m.recordManualThrow(T(50)); // bull marks 2
    m.recordManualThrow(T(50)); // bull marks 4 => closed; solo => win (no points needed)
    auto gs = m.snapshot();
    CHECK(P(gs,0).marks[6]>=3);
    CHECK(gs.gameOver==true);
    CHECK(gs.winner.has_value() && *gs.winner==0);
    CHECK(gs.finishedPlayers.size()==1 && gs.finishedPlayers[0]==0);
    CHECK(P(gs,0).score==0); // no opponents => overflow banks nothing
}

int main(){
    closeExact_threeSingles();
    closeExact_doubleThenSingle();
    closeExact_singleTriple();
    closeAndOverflow_sameThrow_20();
    closeAndOverflow_sameThrow_19();
    closeAndOverflow_sameThrow_15();
    overflow_banksWhileOpponentOpen();
    overflow_noPointsWhenAllOppClosed();
    overflow_accumulateAcrossTurns();
    nonTarget_ignored();
    bull_closeAndOverflow();
    threePlayer_banksWhileAnyOpen();
    closedAllButTrailing_thenLead();
    soloExactWin();
    if(!g_failures) std::cout<<"all cricket_scoring cases passed\n";
    else std::cerr<<g_failures<<" check(s) failed\n";
    return g_failures;
}
