// Nasty-case simulation tests: correctThrow editing PAST Cricket throws.
// Asserts CORRECT replay semantics (may fail against current buggy engine).
#include "game/GameManager.hpp"
#include <iostream>
#include <string>
#include <vector>
using namespace dart::game;
static int g_failures = 0;
#define CHECK(cond) do { if(!(cond)){ ++g_failures; \
  std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<"  "<<#cond<<"\n"; } } while(0)
static Throw T(int v, int mult=1){ return Throw{v, mult, false}; }

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

// marks[0]=20, [1]=19, [2]=18, [3]=17, [4]=16, [5]=15, [6]=bull.
static void assertMarksSane(const GameState& gs){
    for (const auto& p : gs.players)
        for (size_t i=0;i<p.marks.size();++i)
            CHECK(p.marks[i] >= 0 && p.marks[i] <= 3);
}
static void feed(GameManager& m, std::vector<Throw> ts){
    for (auto& t : ts) m.recordManualThrow(t);
}

// ── Case 1: an EARLIER miss becomes a close; marks appear only for thrower. ──
static void case_missToClose(){
    GameManager m; newCricket(m, 2);
    // P0 visit: three non-target misses (value 1 is not a target).
    feed(m, {T(1), T(1), T(1)});
    // P1 visit: three misses.
    feed(m, {T(1), T(1), T(1)});
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==0);
    // Correct P0's first dart (turn 0, throw 0) into a triple-20 close.
    m.correctThrow(0, 0, T(20,3));
    gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==3);      // 20 closed, exactly 3 marks
    CHECK(gs.players[0].score==0);          // no overflow -> no points
    CHECK(gs.players[1].marks[0]==0);       // opponent untouched
    CHECK(gs.players[1].score==0);
    assertMarksSane(gs);
}

// ── Case 2: an earlier close becomes a miss; the number reopens. ──
static void case_closeToMiss(){
    GameManager m; newCricket(m, 2);
    feed(m, {T(20,3), T(1), T(1)});   // P0 closes 20
    feed(m, {T(1), T(1), T(1)});      // P1 misses
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==3);
    m.correctThrow(0, 0, T(1));        // turn P0's close into a miss
    gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==0);  // reopened
    CHECK(gs.players[0].score==0);
    CHECK(gs.players[1].marks[0]==0);
    CHECK(gs.players[1].score==0);
    assertMarksSane(gs);
}

// ── Case 3: triple->single on a number closed WITH overflow; score drops. ──
static void case_tripleToSingleOverflow(){
    GameManager m; newCricket(m, 2);
    // P0: close 20 with a triple, then overflow another triple (opp open -> bank 60).
    feed(m, {T(20,3), T(20,3), T(1)});
    feed(m, {T(1), T(1), T(1)});       // P1 stays open on 20
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==3);
    CHECK(gs.players[0].score==60);    // 3 overflow marks * 20
    // Correct the SECOND triple (turn 0, throw 1) to a single -> 1 overflow.
    m.correctThrow(0, 1, T(20));
    gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==3);
    CHECK(gs.players[0].score==20);    // 1 overflow * 20
    CHECK(gs.players[1].score==0);
    assertMarksSane(gs);
}

// ── Case 4: editing a dart in P0's first turn leaves P1 completely untouched. ──
static void case_noCrossPlayerLeak(){
    GameManager m; newCricket(m, 2);
    feed(m, {T(20,3), T(19,3), T(18,3)}); // P0 closes 20,19,18
    feed(m, {T(20,3), T(1), T(1)});        // P1 closes 20
    auto before = m.snapshot();
    std::vector<int> p1marks = before.players[1].marks;
    int p1score = before.players[1].score;
    // Edit P0 throw0 (close 20 -> close 17 instead).
    m.correctThrow(0, 0, T(17,3));
    auto gs = m.snapshot();
    // P1 marks/score unchanged (P1 closed 20 independently; P0 no longer on 20).
    CHECK(gs.players[1].marks == p1marks);
    CHECK(gs.players[1].score == p1score);
    CHECK(gs.players[0].marks[0]==0);  // P0 20 reopened
    CHECK(gs.players[0].marks[3]==3);  // P0 17 now closed (index 3)
    assertMarksSane(gs);
}

// ── Case 5: edit changes who banks overflow (opponent open/closed flips). ──
static void case_overflowBankFlips(){
    GameManager m; newCricket(m, 2);
    feed(m, {T(20,3), T(1), T(1)});    // turn0 P0 closes 20
    feed(m, {T(20,3), T(1), T(1)});    // turn1 P1 closes 20
    feed(m, {T(20,3), T(1), T(1)});    // turn2 P0 overflow 20 but P1 closed -> no bank
    auto gs = m.snapshot();
    CHECK(gs.players[0].score==0);
    // Reopen P1's 20 by correcting turn1 throw0 to a miss.
    m.correctThrow(1, 0, T(1));
    gs = m.snapshot();
    CHECK(gs.players[1].marks[0]==0);  // P1 open on 20
    CHECK(gs.players[0].marks[0]==3);
    CHECK(gs.players[0].score==60);    // now the overflow banks (P1 open)
    assertMarksSane(gs);
}

// ── Case 6: two sequential corrections stay coherent; no inflation. ──
static void case_repeatedCorrections(){
    GameManager m; newCricket(m, 2);
    feed(m, {T(20,3), T(20,3), T(1)}); // P0 close + overflow(60)
    feed(m, {T(1), T(1), T(1)});
    m.correctThrow(0, 1, T(20));       // overflow triple -> single: score 20
    auto gs = m.snapshot();
    CHECK(gs.players[0].score==20);
    m.correctThrow(0, 0, T(20));       // close triple -> single: 1+1 marks, none overflow
    gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==2);  // single + single = 2 marks, not closed
    CHECK(gs.players[0].score==0);     // never closed -> no overflow banked
    CHECK(gs.players[1].score==0);
    assertMarksSane(gs);
}

// ── Case 7: cut-throat: edit a dump-causing throw; opponent recomputes. ──
static void case_cutThroatDump(){
    GameManager m; newCricket(m, 2, /*cutThroat=*/true);
    feed(m, {T(20,3), T(20,3), T(1)}); // P0 close + overflow -> dump 60 onto P1
    feed(m, {T(1), T(1), T(1)});
    auto gs = m.snapshot();
    CHECK(gs.players[0].score==0);     // thrower gains nothing in cut-throat
    CHECK(gs.players[1].score==60);    // dumped onto open opponent
    m.correctThrow(0, 1, T(20));       // overflow triple -> single
    gs = m.snapshot();
    CHECK(gs.players[0].score==0);
    CHECK(gs.players[1].score==20);    // recomputed dump
    assertMarksSane(gs);
}

// ── Case 8: cut-throat dump target closes -> no more dumping after correction. ──
static void case_cutThroatOpponentCloses(){
    GameManager m; newCricket(m, 2, /*cutThroat=*/true);
    feed(m, {T(20,3), T(1), T(1)});    // turn0 P0 close 20
    feed(m, {T(1), T(1), T(1)});        // turn1 P1 misses (stays open)
    feed(m, {T(20,3), T(1), T(1)});    // turn2 P0 overflow -> dump 60 onto open P1
    auto gs = m.snapshot();
    CHECK(gs.players[1].score==60);
    // Make P1 close 20 in turn1 -> at turn2 P1 already closed, no dump.
    m.correctThrow(1, 0, T(20,3));
    gs = m.snapshot();
    CHECK(gs.players[1].marks[0]==3);
    CHECK(gs.players[1].score==0);     // no dump (P1 closed before overflow)
    CHECK(gs.players[0].score==0);
    assertMarksSane(gs);
}

// ── Case 9: non-target -> other non-target is a no-op on marks/score. ──
static void case_nonTargetNoOp(){
    GameManager m; newCricket(m, 2);
    feed(m, {T(7), T(13), T(14)});     // all non-target
    feed(m, {T(1), T(1), T(1)});
    auto before = m.snapshot();
    m.correctThrow(0, 0, T(13));        // 7 -> 13, still non-target
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks == before.players[0].marks);
    CHECK(gs.players[0].score == before.players[0].score);
    CHECK(gs.players[1].marks == before.players[1].marks);
    assertMarksSane(gs);
}

// ── Case 10: marks never exceed 3 across repeated close attempts + edit. ──
static void case_marksNeverExceedThree(){
    GameManager m; newCricket(m, 2);
    feed(m, {T(20), T(20,3), T(20,3)}); // 1 + close(2)+1of -> then +3 overflow
    feed(m, {T(1), T(1), T(1)});         // P1 open on 20
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==3);
    // single(1) then triple(closes, 1 overflow=20) then triple(3 overflow=60) => 80.
    CHECK(gs.players[0].score==80);
    m.correctThrow(0, 0, T(20,3));      // first dart -> triple: closes immediately
    gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==3);   // still capped at 3
    // triple close(3), triple overflow(60), triple overflow(60) = 120
    CHECK(gs.players[0].score==120);
    assertMarksSane(gs);
}

// ── Case 11: 3-player standard — overflow banks only to thrower, others zero. ──
static void case_threePlayerNoLeak(){
    GameManager m; newCricket(m, 3);
    feed(m, {T(20,3), T(20,3), T(1)});  // P0 close + overflow(60): opps open
    feed(m, {T(1), T(1), T(1)});         // P1
    feed(m, {T(1), T(1), T(1)});         // P2
    auto gs = m.snapshot();
    CHECK(gs.players[0].score==60);
    CHECK(gs.players[1].score==0);
    CHECK(gs.players[2].score==0);
    // Correct overflow triple -> single.
    m.correctThrow(0, 1, T(20));
    gs = m.snapshot();
    CHECK(gs.players[0].score==20);
    CHECK(gs.players[1].score==0);
    CHECK(gs.players[2].score==0);
    assertMarksSane(gs);
}

// ── Case 12: edit a later dart doesn't retroactively steal earlier points. ──
static void case_pointsStayWithOwner(){
    GameManager m; newCricket(m, 2);
    feed(m, {T(20,3), T(20,3), T(1)});  // turn0 P0: close + overflow 60
    feed(m, {T(19,3), T(19,3), T(1)});  // turn1 P1: close 19 + overflow 57
    auto gs = m.snapshot();
    CHECK(gs.players[0].score==60);
    CHECK(gs.players[1].score==57);
    // Correct P1's overflow (turn1 throw1) triple19 -> single19 => overflow 19.
    m.correctThrow(1, 1, T(19));
    gs = m.snapshot();
    CHECK(gs.players[0].score==60);     // P0 untouched
    CHECK(gs.players[1].score==19);     // P1 recomputed, not stolen by P0
    CHECK(gs.players[0].marks[0]==3);
    CHECK(gs.players[1].marks[1]==3);   // 19 index 1
    assertMarksSane(gs);
}

int main(){
    case_missToClose();
    case_closeToMiss();
    case_tripleToSingleOverflow();
    case_noCrossPlayerLeak();
    case_overflowBankFlips();
    case_repeatedCorrections();
    case_cutThroatDump();
    case_cutThroatOpponentCloses();
    case_nonTargetNoOp();
    case_marksNeverExceedThree();
    case_threePlayerNoLeak();
    case_pointsStayWithOwner();
    if(!g_failures) std::cout<<"all cricket_correct_past cases passed\n";
    else std::cerr<<g_failures<<" check(s) failed\n";
    return g_failures;
}
