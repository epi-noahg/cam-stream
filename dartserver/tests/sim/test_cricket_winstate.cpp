// Nasty-case simulations: correctThrow that FLIPS Cricket win/gameOver state,
// both directions, standard & cut-throat, 2 solo players.
//
// These pin down the CORRECT expected behavior. The engine is known-buggy
// (Cricket correctThrow does not rebuild winner/finished/gameOver), so several
// of these assertions are EXPECTED to fail against today's code.
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

// ── helpers ──────────────────────────────────────────────────────────────
static void rec(GameManager& m, std::initializer_list<Throw> ts){
    for (const Throw& t : ts) m.recordManualThrow(t);
}
static bool has(const std::vector<int>& v, int x){
    for (int e : v) if (e==x) return true; return false;
}
static const Throw T20t = T(20,3);  // triple 20 (closes 20 outright)
static const Throw MISS  = T(3);    // non-target dart

// A player who has closed everything and is not blocked on points.
static void expectClosedAll(const PlayerState& p){
    for (int i=0;i<7;i++) CHECK(p.marks.size()==7 && p.marks[i]==3);
}

// Build a game where P0 has closed every target and WON on a tie (score 0=0).
// Standard variant. Turn layout: t0/t2/t4 = P0, t1/t3 = P1.
static void buildStandardWin(GameManager& m){
    newCricket(m, 2, /*cutThroat=*/false);
    rec(m, {T(20,3),T(19,3),T(18,3)});   // t0 P0 closes 20,19,18
    rec(m, {MISS,MISS,MISS});            // t1 P1
    rec(m, {T(17,3),T(16,3),T(15,3)});   // t2 P0 closes 17,16,15
    rec(m, {MISS,MISS,MISS});            // t3 P1
    rec(m, {T(25),T(25),T(25)});         // t4 P0 closes bull -> win on 3rd dart
}

// ── cases ──────────────────────────────────────────────────────────────

// 1. Finished standard game; edit the WINNING dart into a miss -> reopen.
static void standard_reopen_winning_dart_to_miss(){
    GameManager m; buildStandardWin(m);
    GameState g = m.snapshot();
    CHECK(g.gameOver); CHECK(g.winner.has_value() && *g.winner==0);
    CHECK(has(g.finishedPlayers,0) && has(g.finishedPlayers,1));
    // winning dart = last dart of the last (winning) turn.
    CHECK(g.turns.size()>4 && g.turns[4].size()==3);
    m.correctThrow(4, 2, MISS);          // 25 -> miss
    GameState r = m.snapshot();
    CHECK(!r.gameOver);
    CHECK(!r.winner.has_value());
    CHECK(r.finishedPlayers.empty());
    CHECK(r.players[0].marks[6]==2);     // bull now only 2 marks
    for (int i=0;i<6;i++) CHECK(r.players[0].marks[i]==3);
}

// 2. Unfinished game one dart short of the bull; correct the miss into the
//    closing dart -> game becomes OVER with correct winner.
static void standard_finish_from_one_short(){
    GameManager m; newCricket(m, 2, false);
    rec(m, {T(20,3),T(19,3),T(18,3)});   // t0
    rec(m, {MISS,MISS,MISS});            // t1
    rec(m, {T(17,3),T(16,3),T(15,3)});   // t2
    rec(m, {MISS,MISS,MISS});            // t3
    rec(m, {T(25),T(25),MISS});          // t4 bull=2 marks, NOT closed
    GameState g = m.snapshot();
    CHECK(!g.gameOver); CHECK(!g.winner.has_value());
    CHECK(g.players[0].marks[6]==2);
    m.correctThrow(4, 2, T(25));         // miss -> 25 closes bull -> win
    GameState r = m.snapshot();
    CHECK(r.gameOver);
    CHECK(r.winner.has_value() && *r.winner==0);
    CHECK(has(r.finishedPlayers,0) && has(r.finishedPlayers,1));
    expectClosedAll(r.players[0]);
}

// 3. Harmless edit in a finished game (does NOT affect the win) -> stays won.
//    Regression for 'edit un-finishes a won game'.
static void standard_harmless_edit_stays_finished(){
    GameManager m; buildStandardWin(m);
    m.correctThrow(1, 0, T(7));          // P1 non-target miss 3 -> 7 (harmless)
    GameState r = m.snapshot();
    CHECK(r.gameOver);
    CHECK(r.winner.has_value() && *r.winner==0);
    CHECK(has(r.finishedPlayers,0) && has(r.finishedPlayers,1));
    expectClosedAll(r.players[0]);
    CHECK(r.players[0].score==0 && r.players[1].score==0);
}

// 4+5. Standard: P0 closes everything but TRAILS on points -> NOT finished.
//     Then flip the lead (remove opponent's points) -> finishes.
static void standard_trailing_not_finished_then_flip(){
    GameManager m; newCricket(m, 2, false);
    rec(m, {T(19,3),T(18,3),T(17,3)});   // t0 P0 closes 19,18,17 (20 still open)
    rec(m, {T20t,T(20,2),MISS});         // t1 P1: T20 closes, D20 overflow -> +40
    rec(m, {T(20,3),T(16,3),T(15,3)});   // t2 P0 closes 20,16,15
    rec(m, {MISS,MISS,MISS});            // t3 P1
    rec(m, {T(25),T(25),T(25)});         // t4 P0 closes bull; score 0 < 40
    GameState g = m.snapshot();
    CHECK(!g.gameOver); CHECK(!g.winner.has_value());
    CHECK(g.finishedPlayers.empty());
    expectClosedAll(g.players[0]);       // closed all yet not finished
    CHECK(g.players[0].score==0);
    CHECK(g.players[1].score==40);
    // Remove P1's overflow: D20 -> miss. Now P0 ties 0=0 -> P0 wins.
    m.correctThrow(1, 1, MISS);
    GameState r = m.snapshot();
    CHECK(r.gameOver);
    CHECK(r.winner.has_value() && *r.winner==0);
    CHECK(has(r.finishedPlayers,0) && has(r.finishedPlayers,1));
    CHECK(r.players[1].score==0);
}

// 6. Finished standard game; edit gives the OPPONENT points so P0 now trails
//    -> game un-finishes.
static void standard_edit_gives_opp_points_unfinishes(){
    GameManager m; newCricket(m, 2, false);
    rec(m, {T(19,3),T(18,3),T(17,3)});   // t0 P0 closes 19,18,17
    rec(m, {T(20,2),MISS,MISS});         // t1 P1: D20 -> marks[20]=2 (no overflow)
    rec(m, {T(20,3),T(16,3),T(15,3)});   // t2 P0 closes 20,16,15
    rec(m, {MISS,MISS,MISS});            // t3 P1
    rec(m, {T(25),T(25),T(25)});         // t4 P0 closes bull; 0=0 tie -> win
    GameState g = m.snapshot();
    CHECK(g.gameOver && g.winner.has_value() && *g.winner==0);
    // Turn P1's 2nd dart into T20: with 2 pre-marks it closes(1)+overflow(2)
    // -> P1 banks 40 while P0's 20 is still open at t1.
    m.correctThrow(1, 1, T20t);
    GameState r = m.snapshot();
    CHECK(!r.gameOver);
    CHECK(!r.winner.has_value());
    CHECK(r.finishedPlayers.empty());
    CHECK(r.players[1].score==40);
    CHECK(r.players[0].score==0);
}

// 7. Cut-throat: finished game; edit dumps points onto P0 so P0 no longer has
//    the fewest -> game reopens.
static void cutthroat_reopen_via_dump(){
    GameManager m; newCricket(m, 2, /*cutThroat=*/true);
    rec(m, {T(19,3),T(18,3),T(17,3)});   // t0 P0
    rec(m, {T(20,2),MISS,MISS});         // t1 P1: D20 -> marks[20]=2, no dump
    rec(m, {T(20,3),T(16,3),T(15,3)});   // t2 P0 closes 20,16,15
    rec(m, {MISS,MISS,MISS});            // t3 P1
    rec(m, {T(25),T(25),T(25)});         // t4 P0 closes bull; 0<=0 -> win
    GameState g = m.snapshot();
    CHECK(g.gameOver && g.winner.has_value() && *g.winner==0);
    CHECK(g.players[0].score==0 && g.players[1].score==0);
    // P1's 2nd dart -> T20: closes(1)+overflow(2) dumps 40 onto P0 (open at t1).
    m.correctThrow(1, 1, T20t);
    GameState r = m.snapshot();
    CHECK(!r.gameOver);
    CHECK(!r.winner.has_value());
    CHECK(r.finishedPlayers.empty());
    CHECK(r.players[0].score==40);       // dumped onto P0
    CHECK(r.players[1].score==0);
}

// 8+9. Cut-throat: P0 closes everything but has MORE points (dumped on) -> NOT
//     finished. Removing the dump flips P0 to the win (fewest points).
static void cutthroat_trailing_not_finished_then_flip(){
    GameManager m; newCricket(m, 2, true);
    rec(m, {T(19,3),T(18,3),T(17,3)});   // t0 P0
    rec(m, {T20t,T(20,2),MISS});         // t1 P1: T20 closes, D20 dumps 40 on P0
    rec(m, {T(20,3),T(16,3),T(15,3)});   // t2 P0 closes 20,16,15
    rec(m, {MISS,MISS,MISS});            // t3 P1
    rec(m, {T(25),T(25),T(25)});         // t4 P0 closes bull; 40<=0 false
    GameState g = m.snapshot();
    CHECK(!g.gameOver); CHECK(!g.winner.has_value());
    CHECK(g.finishedPlayers.empty());
    expectClosedAll(g.players[0]);
    CHECK(g.players[0].score==40);
    m.correctThrow(1, 1, MISS);          // remove dump -> P0 score 0 <= 0 -> win
    GameState r = m.snapshot();
    CHECK(r.gameOver);
    CHECK(r.winner.has_value() && *r.winner==0);
    CHECK(has(r.finishedPlayers,0) && has(r.finishedPlayers,1));
    CHECK(r.players[0].score==0);
}

// 10. Cut-throat harmless edit -> stays finished.
static void cutthroat_harmless_edit_stays_finished(){
    GameManager m; newCricket(m, 2, true);
    rec(m, {T(20,3),T(19,3),T(18,3)});   // t0
    rec(m, {MISS,MISS,MISS});            // t1
    rec(m, {T(17,3),T(16,3),T(15,3)});   // t2
    rec(m, {MISS,MISS,MISS});            // t3
    rec(m, {T(25),T(25),T(25)});         // t4 win
    GameState g = m.snapshot();
    CHECK(g.gameOver && g.winner.has_value() && *g.winner==0);
    m.correctThrow(3, 0, T(7));          // harmless non-target edit
    GameState r = m.snapshot();
    CHECK(r.gameOver);
    CHECK(r.winner.has_value() && *r.winner==0);
    CHECK(has(r.finishedPlayers,0) && has(r.finishedPlayers,1));
}

// 11. After a win, recordManualThrow is a no-op.
static void record_after_win_is_noop(){
    GameManager m; buildStandardWin(m);
    GameState before = m.snapshot();
    m.recordManualThrow(T(1,3));
    m.recordManualThrow(T(25));
    GameState after = m.snapshot();
    CHECK(after.gameOver);
    CHECK(after.winner==before.winner);
    CHECK(after.finishedPlayers==before.finishedPlayers);
    CHECK(after.players.size()==before.players.size());
    for (size_t i=0;i<after.players.size();++i){
        CHECK(after.players[i].score==before.players[i].score);
        CHECK(after.players[i].marks==before.players[i].marks);
    }
}

// 12. undo restores the finished state after a reopening correctThrow.
static void undo_restores_finished_after_correct(){
    GameManager m; buildStandardWin(m);
    m.correctThrow(4, 2, MISS);          // reopen
    GameState open = m.snapshot();
    CHECK(!open.gameOver && !open.winner.has_value());
    m.undo();                            // revert the correction
    GameState r = m.snapshot();
    CHECK(r.gameOver);
    CHECK(r.winner.has_value() && *r.winner==0);
    CHECK(has(r.finishedPlayers,0) && has(r.finishedPlayers,1));
    expectClosedAll(r.players[0]);
}

int main(){
    standard_reopen_winning_dart_to_miss();
    standard_finish_from_one_short();
    standard_harmless_edit_stays_finished();
    standard_trailing_not_finished_then_flip();
    standard_edit_gives_opp_points_unfinishes();
    cutthroat_reopen_via_dump();
    cutthroat_trailing_not_finished_then_flip();
    cutthroat_harmless_edit_stays_finished();
    record_after_win_is_noop();
    undo_restores_finished_after_correct();
    if(!g_failures) std::cout<<"all cricket_winstate cases passed\n";
    else std::cerr<<g_failures<<" check(s) failed\n";
    return g_failures;
}
