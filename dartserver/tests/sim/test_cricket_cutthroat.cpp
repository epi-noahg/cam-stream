// Cut-throat Cricket nasty-case simulations.
// Asserts AUTHORITATIVE cut-throat rules; expected to fail vs current buggy code.
#include "game/GameManager.hpp"
#include <iostream>
#include <string>
#include <vector>
using namespace dart::game;
static int g_failures = 0;
#define CHECK(cond) do { if(!(cond)){ ++g_failures; \
  std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<"  "<<#cond<<"\n"; } } while(0)
static Throw T(int v, int mult=1){ return Throw{v, mult, false}; }

// Build+start a cut-throat Cricket game in-place (solo players).
static void newCricket(GameManager& m, int nplayers, bool cutThroat=true,
                       bool useBull=true, std::vector<int> teamOf={}) {
    std::vector<PlayerState> ps;
    for (int i=0;i<nplayers;i++){ PlayerState p; p.id=i; p.nickname="P"+std::to_string(i);
        p.team = (i<(int)teamOf.size()?teamOf[i]:0); ps.push_back(p);}
    GameConfig cfg; cfg.mode=GameMode::Cricket;
    cfg.cricket.cutThroat=cutThroat; cfg.cricket.useBull=useBull;
    cfg.cricket.teams = teamOf.empty()?1:2;
    m.createGame(std::move(ps), cfg);
}

// Record three T(7) misses to finish the current player's visit (7 is a non-target).
static void missTurn(GameManager& m){ for(int i=0;i<3;i++) m.recordManualThrow(T(7)); }

// ── cases ───────────────────────────────────────────────────────────────────

// Overflow on 20 dumps value*overflowMarks onto the open opponent; thrower stays 0.
static void overflowDumpsOnOpenOpponent(){
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20,3));   // close 20 (3 marks)
    m.recordManualThrow(T(20,3));   // 3 overflow -> 3*20=60 onto P1
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==3);
    CHECK(gs.players[0].score==0);   // no self-dump
    CHECK(gs.players[1].score==60);
    CHECK(gs.players[1].marks[0]==0);
    CHECK(!gs.gameOver);
}

// Thrower never receives their own overflow points (explicit no-self-dump).
static void noSelfDump(){
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20,3));   // close
    m.recordManualThrow(T(20,3));   // overflow
    m.recordManualThrow(T(20,3));   // more overflow
    auto gs = m.snapshot();
    CHECK(gs.players[0].score==0);
    CHECK(gs.players[1].score==120); // 6 overflow marks * 20
}

// An opponent who already closed the number receives nothing.
static void closedOpponentReceivesNothing(){
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); // P0 close 20
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); // P1 close 20
    m.recordManualThrow(T(20,3)); // P0 overflow but P1 already closed 20 -> nothing
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==3);
    CHECK(gs.players[1].marks[0]==3);
    CHECK(gs.players[0].score==0);
    CHECK(gs.players[1].score==0);
}

// Close-and-overflow in the same visit: only the overflow portion is dumped.
static void closeAndOverflowSameThrow(){
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20,2));   // double 20 -> 2 marks
    m.recordManualThrow(T(20,3));   // triple -> 1 closes, 2 overflow -> 2*20=40 onto P1
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==3);
    CHECK(gs.players[0].score==0);
    CHECK(gs.players[1].score==40);
}

// 3 players: overflow dumped onto EVERY open opponent simultaneously.
static void threePlayersOverflowMultiple(){
    GameManager m; newCricket(m, 3);
    m.recordManualThrow(T(20,3));   // P0 close 20
    m.recordManualThrow(T(20,3));   // 3 overflow -> P1 and P2 each +60
    auto gs = m.snapshot();
    CHECK(gs.players[0].score==0);
    CHECK(gs.players[1].score==60);
    CHECK(gs.players[2].score==60);
}

// 4 players, all opponents open: each gets value*marks.
static void fourPlayersOverflowAllOpen(){
    GameManager m; newCricket(m, 4);
    m.recordManualThrow(T(20,3));   // close
    m.recordManualThrow(T(20,3));   // overflow 60 to each of P1,P2,P3
    auto gs = m.snapshot();
    CHECK(gs.players[0].score==0);
    CHECK(gs.players[1].score==60);
    CHECK(gs.players[2].score==60);
    CHECK(gs.players[3].score==60);
}

// 3 players, one opponent already closed: only the still-open opponent is charged.
static void mixedOpenClosedOverflow(){
    GameManager m; newCricket(m, 3);
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); // P0 close 20
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); // P1 close 20
    missTurn(m);                                                                         // P2 misses (open)
    m.recordManualThrow(T(20,3)); // P0 overflow -> P1 closed(no), P2 open(+60)
    auto gs = m.snapshot();
    CHECK(gs.players[1].score==0);
    CHECK(gs.players[2].score==60);
    CHECK(gs.players[0].score==0);
}

// Bull overflow in cut-throat: value 25 per overflow mark onto the open opponent.
static void bullOverflowCutThroat(){
    GameManager m; newCricket(m, 2, /*cutThroat*/true, /*useBull*/true);
    m.recordManualThrow(T(50,1));   // inner bull -> 2 marks
    m.recordManualThrow(T(25,1));   // outer bull -> 1 mark, closes bull (3)
    m.recordManualThrow(T(25,1));   // 1 overflow -> 25 onto P1
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[6]==3);
    CHECK(gs.players[0].score==0);
    CHECK(gs.players[1].score==25);
}

// Lowest score wins: closing all with score <= every opponent finishes the game.
static void lowestScoreWins(){
    GameManager m; newCricket(m, 2, /*cutThroat*/true, /*useBull*/false);
    // P0 visit1: close 20,19,18
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(19,3)); m.recordManualThrow(T(18,3));
    missTurn(m); // P1
    // P0 visit2: close 17,16,15 -> all closed, score 0 == opponent 0 -> win
    m.recordManualThrow(T(17,3)); m.recordManualThrow(T(16,3)); m.recordManualThrow(T(15,3));
    auto gs = m.snapshot();
    CHECK(gs.gameOver);
    CHECK(gs.winner.has_value() && gs.winner.value()==0);
    CHECK(gs.finishedPlayers.size()==2);
    CHECK(gs.players[0].score==0);
}

// Closed-all but HIGHEST score is NOT a winner (cut-throat wants lowest).
static void highestScoreNotFinished(){
    GameManager m; newCricket(m, 2, /*cutThroat*/true, /*useBull*/false);
    // P0 visit1: close 20,19,18
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(19,3)); m.recordManualThrow(T(18,3));
    // P1 visit: close 15 then overflow 15 onto P0 (P0 open on 15) -> P0 +45
    m.recordManualThrow(T(15,3)); m.recordManualThrow(T(15,3)); m.recordManualThrow(T(7));
    // P0 visit2: close 17,16,15 -> all closed but score 45 > opponent 0 -> NOT finished
    m.recordManualThrow(T(17,3)); m.recordManualThrow(T(16,3)); m.recordManualThrow(T(15,3));
    auto gs = m.snapshot();
    CHECK(!gs.gameOver);
    CHECK(!gs.winner.has_value());
    CHECK(gs.finishedPlayers.empty());
    CHECK(gs.players[0].score==45);
    // all P0 numbers closed
    for(int i=0;i<6;i++) CHECK(gs.players[0].marks[i]==3);
}

// Undo reverts an overflow dump exactly.
static void undoRevertsOverflow(){
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20,3));   // close 20
    m.recordManualThrow(T(20,3));   // overflow -> P1 +60
    CHECK(m.snapshot().players[1].score==60);
    m.undo();                       // revert the overflow dart
    auto gs = m.snapshot();
    CHECK(gs.players[1].score==0);
    CHECK(gs.players[0].marks[0]==3);
    CHECK(gs.players[0].score==0);
}

// After a win, further throws are no-ops.
static void gameOverNoMoreThrows(){
    GameManager m; newCricket(m, 2, /*cutThroat*/true, /*useBull*/false);
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(19,3)); m.recordManualThrow(T(18,3));
    missTurn(m);
    m.recordManualThrow(T(17,3)); m.recordManualThrow(T(16,3)); m.recordManualThrow(T(15,3)); // win
    auto before = m.snapshot();
    CHECK(before.gameOver);
    m.recordManualThrow(T(19,3)); // should be ignored
    auto after = m.snapshot();
    CHECK(after.gameOver);
    CHECK(after.winner==before.winner);
    CHECK(after.players[1].score==before.players[1].score);
}

int main(){
    overflowDumpsOnOpenOpponent();
    noSelfDump();
    closedOpponentReceivesNothing();
    closeAndOverflowSameThrow();
    threePlayersOverflowMultiple();
    fourPlayersOverflowAllOpen();
    mixedOpenClosedOverflow();
    bullOverflowCutThroat();
    lowestScoreWins();
    highestScoreNotFinished();
    undoRevertsOverflow();
    gameOverNoMoreThrows();
    if(!g_failures) std::cout<<"all cutthroat cases passed\n";
    else std::cerr<<g_failures<<" check(s) failed\n";
    return g_failures;
}
