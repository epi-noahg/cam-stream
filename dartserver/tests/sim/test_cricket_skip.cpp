// Nasty-case simulation tests: Cricket + nextPlayer (skip) combined with
// correctThrow / undo.  Reproduces the misattribution bug where, after a skip,
// an edit sends a player's marks/points to the WRONG player.
//
// Asserts ONLY observable per-player state (score / marks / target) and match
// flags (winner / gameOver / finishedPlayers).  Never asserts turn layout.
#include "game/GameManager.hpp"
#include <iostream>
#include <string>
#include <vector>
using namespace dart::game;

static int g_failures = 0;
#define CHECK(cond) do { if(!(cond)){ ++g_failures; \
  std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<"  "<<#cond<<"\n"; } } while(0)

static Throw T(int v, int mult=1){ return Throw{v, mult, false}; }

// Build+start a Cricket game in-place.
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

// marks[] index: 0->20, 1->19, 2->18, 3->17, 4->16, 5->15, 6->bull(25/50).
static bool marksEq(const PlayerState& p, std::vector<int> want){
    if((int)p.marks.size()!=7) return false;
    for(int i=0;i<7;i++) if(p.marks[i]!=want[i]) return false;
    return true;
}
// Every mark value must be within the legal 0..3 range.
static void checkMarksBounded(const GameState& gs){
    for(const auto& p : gs.players)
        for(int mk : p.marks) CHECK(mk>=0 && mk<=3);
}

// ───────────────────────────────────────────────────────────────────────────
// 1. P0 throws 2 darts then nextPlayer (skips rest); P1 plays.  Marks land on
//    the right players; nothing leaks across the skip.
static void case_skip_midturn_two_darts(){
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20)); // P0 marks[0]=1
    m.recordManualThrow(T(20)); // P0 marks[0]=2
    m.nextPlayer();             // skip P0's 3rd dart -> P1 up
    m.recordManualThrow(T(19)); // P1 marks[1]=1
    auto gs = m.snapshot();
    CHECK(marksEq(gs.players[0], {2,0,0,0,0,0,0}));
    CHECK(marksEq(gs.players[1], {0,1,0,0,0,0,0}));
    CHECK(gs.players[0].score==0 && gs.players[1].score==0);
    CHECK(!gs.gameOver);
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 2. EXACT reported repro: full P0 turn (auto hand-over), skip P1 entirely,
//    P0 plays again, then correctThrow an EARLIER P0 dart.  P0 must keep its
//    own marks; NOTHING leaks to the (skipped) P1.
static void case_skip_then_correct_earlier_no_leak(){
    GameManager m; newCricket(m, 2);
    // P0 turn 1: closes 20.
    m.recordManualThrow(T(20));
    m.recordManualThrow(T(20));
    m.recordManualThrow(T(20)); // auto hand-over to P1
    m.nextPlayer();             // skip P1 entirely -> P0 up again
    // P0 turn 2: closes 19.
    m.recordManualThrow(T(19));
    m.recordManualThrow(T(19));
    m.recordManualThrow(T(19));
    // Correct P0's very first dart (turn 0, dart 0): 20 -> 18.
    m.correctThrow(0, 0, T(18));
    auto gs = m.snapshot();
    // P0: two 20s + one 18 + three 19s.
    CHECK(marksEq(gs.players[0], {2,3,1,0,0,0,0}));
    CHECK(gs.players[0].score==0);
    // P1 was skipped and NEVER threw: everything must stay zero.
    CHECK(marksEq(gs.players[1], {0,0,0,0,0,0,0}));
    CHECK(gs.players[1].score==0);
    CHECK(!gs.gameOver && !gs.winner.has_value());
    CHECK(gs.finishedPlayers.empty());
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 3. Skip at game start: nextPlayer before any dart -> P1 leads off.
static void case_skip_at_start(){
    GameManager m; newCricket(m, 2);
    m.nextPlayer();             // skip P0 before any dart
    m.recordManualThrow(T(20)); // this is P1's dart
    auto gs = m.snapshot();
    CHECK(marksEq(gs.players[0], {0,0,0,0,0,0,0}));
    CHECK(marksEq(gs.players[1], {1,0,0,0,0,0,0}));
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 4. Multiple consecutive nextPlayer calls with 2 players wrap back to P0.
static void case_multiple_consecutive_skips_two_players(){
    GameManager m; newCricket(m, 2);
    m.nextPlayer();  // -> P1
    m.nextPlayer();  // -> P0 again
    m.recordManualThrow(T(19)); // must be P0
    auto gs = m.snapshot();
    CHECK(marksEq(gs.players[0], {0,1,0,0,0,0,0}));
    CHECK(marksEq(gs.players[1], {0,0,0,0,0,0,0}));
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 5. Three players, several skips, then correctThrow deep in history.  Each
//    player keeps exactly their own marks.
static void case_three_players_skips_then_correct(){
    GameManager m; newCricket(m, 3);
    // P0 turn 1: one dart then skip.
    m.recordManualThrow(T(20)); // P0 marks[0]=1
    m.nextPlayer();             // -> P1
    // P1 turn: two darts then skip.
    m.recordManualThrow(T(19)); // P1 marks[1]=1
    m.recordManualThrow(T(19)); // P1 marks[1]=2
    m.nextPlayer();             // -> P2
    // P2 turn: closes 18.
    m.recordManualThrow(T(18));
    m.recordManualThrow(T(18));
    m.recordManualThrow(T(18)); // P2 marks[2]=3, auto hand-over -> P0
    // P0 turn 2: adds a 17.
    m.recordManualThrow(T(17)); // P0 marks[3]=1
    // Sanity before edit.
    auto pre = m.snapshot();
    CHECK(marksEq(pre.players[0], {1,0,0,1,0,0,0}));
    CHECK(marksEq(pre.players[1], {0,2,0,0,0,0,0}));
    CHECK(marksEq(pre.players[2], {0,0,3,0,0,0,0}));
    // Correct P0's very first dart (turn 0, dart 0): 20 -> 16.
    m.correctThrow(0, 0, T(16));
    auto gs = m.snapshot();
    CHECK(marksEq(gs.players[0], {0,0,0,1,1,0,0})); // 20 gone, 16 added, 17 kept
    CHECK(marksEq(gs.players[1], {0,2,0,0,0,0,0})); // untouched
    CHECK(marksEq(gs.players[2], {0,0,3,0,0,0,0})); // untouched
    CHECK(gs.players[0].score==0 && gs.players[1].score==0 && gs.players[2].score==0);
    CHECK(!gs.gameOver);
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 6. undo of a nextPlayer restores the prior current player and state.
static void case_undo_nextplayer_restores_current(){
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20)); // P0 marks[0]=1
    m.nextPlayer();             // -> P1
    m.recordManualThrow(T(19)); // P1 marks[1]=1
    m.undo();                   // revert P1's throw
    {
        auto gs = m.snapshot();
        CHECK(marksEq(gs.players[1], {0,0,0,0,0,0,0}));
    }
    m.undo();                   // revert the nextPlayer -> P0 mid-turn again
    m.recordManualThrow(T(20)); // must attribute to P0 (its turn resumed)
    auto gs = m.snapshot();
    CHECK(marksEq(gs.players[0], {2,0,0,0,0,0,0}));
    CHECK(marksEq(gs.players[1], {0,0,0,0,0,0,0}));
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 7. Interleave: throw, skip, throw, correctThrow, undo.
static void case_interleave_throw_skip_correct_undo(){
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20)); // P0 marks[0]=1
    m.nextPlayer();             // -> P1
    m.recordManualThrow(T(19)); // P1 marks[1]=1
    // Correct P0's first dart: 20 -> 17.
    m.correctThrow(0, 0, T(17));
    {
        auto gs = m.snapshot();
        CHECK(marksEq(gs.players[0], {0,0,0,1,0,0,0}));
        CHECK(marksEq(gs.players[1], {0,1,0,0,0,0,0}));
    }
    // Undo the correction -> back to the pre-correct state.
    m.undo();
    auto gs = m.snapshot();
    CHECK(marksEq(gs.players[0], {1,0,0,0,0,0,0}));
    CHECK(marksEq(gs.players[1], {0,1,0,0,0,0,0}));
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 8. Skip preserves overflow-point attribution: after P0 closes 20 and banks
//    points, a skip of P1 then an edit must keep the points with P0.
static void case_skip_preserves_overflow_points(){
    GameManager m; newCricket(m, 2);
    // P0 turn 1: triple 20 (closes 20), then two single 20 -> 40 overflow pts.
    m.recordManualThrow(T(20,3)); // marks[0]=3 (closed)
    m.recordManualThrow(T(20));   // +20 (P1 hasn't closed 20)
    m.recordManualThrow(T(20));   // +20 -> score 40, auto hand-over
    m.nextPlayer();               // skip P1 -> P0 up again
    m.recordManualThrow(T(19));   // P0 marks[1]=1
    auto gs = m.snapshot();
    CHECK(marksEq(gs.players[0], {3,1,0,0,0,0,0}));
    CHECK(gs.players[0].score==40); // points stay with P0
    CHECK(marksEq(gs.players[1], {0,0,0,0,0,0,0}));
    CHECK(gs.players[1].score==0);  // nothing leaked to skipped P1
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 9. Skip, score, then correct the overflow dart down: points recompute onto
//    P0 only.
static void case_skip_then_correct_overflow_dart(){
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20,3)); // P0 closes 20
    m.recordManualThrow(T(20));   // +20 overflow
    m.recordManualThrow(T(20));   // +20 overflow -> score 40
    m.nextPlayer();               // skip P1
    m.recordManualThrow(T(18));   // P0 marks[2]=1
    // Correct the 2nd dart (turn 0, dart 1): 20 -> 5 (non-target, ignored).
    m.correctThrow(0, 1, T(5));
    auto gs = m.snapshot();
    // Remaining: T20 (3 marks -> closed) + one single 20 overflow (+20).
    CHECK(marksEq(gs.players[0], {3,0,1,0,0,0,0}));
    CHECK(gs.players[0].score==20);
    CHECK(marksEq(gs.players[1], {0,0,0,0,0,0,0}));
    CHECK(gs.players[1].score==0);
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 10. Two skips of the SAME player across turns still attribute correctly.
static void case_repeated_skip_same_player(){
    GameManager m; newCricket(m, 2);
    // P0 turn 1: one dart then skip.
    m.recordManualThrow(T(20));   // P0 marks[0]=1
    m.nextPlayer();               // -> P1
    m.nextPlayer();               // skip P1 entirely -> P0 turn 2
    m.recordManualThrow(T(20));   // P0 marks[0]=2
    m.nextPlayer();               // -> P1
    m.nextPlayer();               // skip P1 again -> P0 turn 3
    m.recordManualThrow(T(20));   // P0 marks[0]=3 (closed)
    auto gs = m.snapshot();
    CHECK(marksEq(gs.players[0], {3,0,0,0,0,0,0}));
    CHECK(marksEq(gs.players[1], {0,0,0,0,0,0,0}));
    CHECK(gs.players[0].score==0 && gs.players[1].score==0);
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 11. Correct a dart INTO a skipped stretch: edit P1's (skipped) turn-0 dart in
//    a 3-player game; only the true owner changes.
static void case_correct_after_multiple_skips_three_players(){
    GameManager m; newCricket(m, 3);
    m.recordManualThrow(T(20)); // P0 t0 d0
    m.recordManualThrow(T(20)); // P0 t0 d1
    m.recordManualThrow(T(20)); // P0 t0 d2 -> closes 20, hand-over P1
    m.nextPlayer();             // skip P1 -> P2
    m.recordManualThrow(T(15)); // P2 marks[5]=1
    m.nextPlayer();             // -> P0
    m.recordManualThrow(T(19)); // P0 marks[1]=1
    // Correct P0's 3rd dart of turn 0 (turn 0, dart 2): 20 -> 19.
    m.correctThrow(0, 2, T(19));
    auto gs = m.snapshot();
    // P0: two 20s + two 19s.
    CHECK(marksEq(gs.players[0], {2,2,0,0,0,0,0}));
    CHECK(marksEq(gs.players[1], {0,0,0,0,0,0,0})); // skipped, untouched
    CHECK(marksEq(gs.players[2], {0,0,0,0,0,1,0})); // untouched
    CHECK(!gs.gameOver);
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 12. undo after a skip+throw sequence rolls back cleanly, then replay differs.
static void case_undo_chain_after_skip(){
    GameManager m; newCricket(m, 2);
    m.recordManualThrow(T(20)); // P0 marks[0]=1
    m.recordManualThrow(T(20)); // P0 marks[0]=2
    m.nextPlayer();             // -> P1
    m.recordManualThrow(T(18)); // P1 marks[2]=1
    m.undo();                   // revert P1 throw
    m.undo();                   // revert nextPlayer -> P0 mid-turn
    m.undo();                   // revert P0 2nd dart
    auto gs = m.snapshot();
    CHECK(marksEq(gs.players[0], {1,0,0,0,0,0,0}));
    CHECK(marksEq(gs.players[1], {0,0,0,0,0,0,0}));
    checkMarksBounded(gs);
}

// ───────────────────────────────────────────────────────────────────────────
// 13. Full independence: P1 scores via overflow after P0 skip; edit P1's dart,
//    P0 stays zero.
static void case_skip_p0_then_p1_scores_and_edit(){
    GameManager m; newCricket(m, 2);
    m.nextPlayer();               // skip P0 at start -> P1
    m.recordManualThrow(T(19,3)); // P1 closes 19
    m.recordManualThrow(T(19));   // +19 overflow (P0 hasn't closed 19)
    m.recordManualThrow(T(19));   // +19 -> P1 score 38
    {
        auto gs = m.snapshot();
        CHECK(marksEq(gs.players[1], {0,3,0,0,0,0,0}));
        CHECK(gs.players[1].score==38);
        CHECK(gs.players[0].score==0);
    }
    // Correct P1's 2nd dart: P0 was skipped at start, so P0 owns the empty
    // turn 0 and P1's darts live in turn 1 -> edit (turnIndex 1, throwIndex 1).
    m.correctThrow(1, 1, T(3));
    auto gs = m.snapshot();
    CHECK(marksEq(gs.players[1], {0,3,0,0,0,0,0}));
    CHECK(gs.players[1].score==19); // only one overflow 19 remains
    CHECK(marksEq(gs.players[0], {0,0,0,0,0,0,0}));
    CHECK(gs.players[0].score==0);
    checkMarksBounded(gs);
}

int main(){
    case_skip_midturn_two_darts();
    case_skip_then_correct_earlier_no_leak();
    case_skip_at_start();
    case_multiple_consecutive_skips_two_players();
    case_three_players_skips_then_correct();
    case_undo_nextplayer_restores_current();
    case_interleave_throw_skip_correct_undo();
    case_skip_preserves_overflow_points();
    case_skip_then_correct_overflow_dart();
    case_repeated_skip_same_player();
    case_correct_after_multiple_skips_three_players();
    case_undo_chain_after_skip();
    case_skip_p0_then_p1_scores_and_edit();
    if(!g_failures) std::cout<<"all cricket_skip cases passed\n";
    else std::cerr<<g_failures<<" check(s) failed\n";
    return g_failures;
}
