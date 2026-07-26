// Nasty-case simulation tests: Cricket undo (exact-revert) semantics.
#include "game/GameManager.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
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

// Compare only OBSERVABLE state: per-player score/marks(all 7)/target + match flags.
static void compare(const GameState& a, const GameState& b) {
    CHECK(a.players.size() == b.players.size());
    size_t n = std::min(a.players.size(), b.players.size());
    for (size_t i=0;i<n;i++){
        const PlayerState& pa=a.players[i]; const PlayerState& pb=b.players[i];
        CHECK(pa.id == pb.id);
        CHECK(pa.score == pb.score);
        CHECK(pa.target == pb.target);
        CHECK(pa.marks.size() == pb.marks.size());
        size_t mm = std::min(pa.marks.size(), pb.marks.size());
        for (size_t k=0;k<mm;k++) CHECK(pa.marks[k] == pb.marks[k]);
    }
    CHECK(a.winner.has_value() == b.winner.has_value());
    if (a.winner.has_value() && b.winner.has_value())
        CHECK(*a.winner == *b.winner);
    CHECK(a.gameOver == b.gameOver);
    CHECK(a.finishedPlayers == b.finishedPlayers);
}

// ── Cases ────────────────────────────────────────────────────────────────

// Undo a single dart thrown mid-turn -> back to game start.
static void undo_single_throw_mid_turn(){
    GameManager m; newCricket(m,2);
    GameState s0 = m.snapshot();
    m.recordManualThrow(T(20));           // 1 mark on 20 for P0
    m.undo();
    compare(s0, m.snapshot());
}

// Undo a throw that CLOSES a number -> marks revert from 3 to 2.
static void undo_closing_throw_reverts_marks(){
    GameManager m; newCricket(m,2);
    m.recordManualThrow(T(20));           // marks[0]=1
    m.recordManualThrow(T(20));           // marks[0]=2
    GameState s0 = m.snapshot();
    CHECK(s0.players[0].marks[0]==2);
    m.recordManualThrow(T(20));           // closes -> marks[0]=3
    CHECK(m.snapshot().players[0].marks[0]==3);
    m.undo();
    compare(s0, m.snapshot());
    CHECK(m.snapshot().players[0].marks[0]==2);
}

// Undo a throw that produced OVERFLOW points (score reverts to 0).
static void undo_overflow_points_reverts_score(){
    GameManager m; newCricket(m,2);       // standard
    m.recordManualThrow(T(20,3));         // closes 20 (3 marks), no overflow yet
    GameState s0 = m.snapshot();
    CHECK(s0.players[0].marks[0]==3);
    CHECK(s0.players[0].score==0);
    m.recordManualThrow(T(20,3));         // 3 overflow marks -> +60 (opp hasn't closed)
    CHECK(m.snapshot().players[0].score==60);
    m.undo();
    compare(s0, m.snapshot());
    CHECK(m.snapshot().players[0].score==0);
}

// Undo close+overflow in the SAME throw.
static void undo_close_and_overflow_same_throw(){
    GameManager m; newCricket(m,2);
    m.recordManualThrow(T(20));           // marks[0]=1
    m.recordManualThrow(T(20));           // marks[0]=2
    GameState s0 = m.snapshot();
    m.recordManualThrow(T(20,3));         // +3 marks -> closes(1) +2 overflow => +40
    CHECK(m.snapshot().players[0].marks[0]==3);
    CHECK(m.snapshot().players[0].score==40);
    m.undo();
    compare(s0, m.snapshot());
    CHECK(m.snapshot().players[0].marks[0]==2);
    CHECK(m.snapshot().players[0].score==0);
}

// Undo the FIRST dart of a new player's turn -> back to previous player's end.
static void undo_across_turn_boundary(){
    GameManager m; newCricket(m,2);
    m.recordManualThrow(T(20,3));         // P0 dart1: close 20
    m.recordManualThrow(T(19,3));         // P0 dart2: close 19
    m.recordManualThrow(T(18,3));         // P0 dart3 -> hand over to P1
    GameState s0 = m.snapshot();
    m.recordManualThrow(T(20,3));         // P1 first dart of new turn
    CHECK(m.snapshot().players[1].marks[0]==3);
    m.undo();
    compare(s0, m.snapshot());
    CHECK(m.snapshot().players[1].marks[0]==0);
}

// Multiple sequential undos rewind all the way back to game start.
static void multiple_sequential_undos_to_start(){
    GameManager m; newCricket(m,2);
    GameState start = m.snapshot();
    m.recordManualThrow(T(20));
    m.recordManualThrow(T(19,2));
    m.recordManualThrow(T(18));           // hand over
    m.recordManualThrow(T(17));           // P1
    m.undo(); m.undo(); m.undo(); m.undo();
    compare(start, m.snapshot());
}

// Undo the WINNING throw: gameOver false, winner cleared, finishedPlayers empty, play resumes.
static void undo_winning_throw(){
    GameManager m; newCricket(m,2);       // standard, useBull=true
    // P0 turn1: close 20,19,18
    m.recordManualThrow(T(20,3));
    m.recordManualThrow(T(19,3));
    m.recordManualThrow(T(18,3));         // -> P1
    m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); // P1 pass -> P0
    // P0 turn2: close 17,16,15
    m.recordManualThrow(T(17,3));
    m.recordManualThrow(T(16,3));
    m.recordManualThrow(T(15,3));         // -> P1
    m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); // P1 pass -> P0
    // P0 turn3: bull. First 50 -> marks[6]=2.
    m.recordManualThrow(T(50));
    GameState s0 = m.snapshot();
    CHECK(s0.players[0].marks[6]==2);
    CHECK(s0.gameOver==false);
    CHECK(!s0.winner.has_value());
    // Second 50 closes bull (all targets closed, score>=opp) -> WIN.
    m.recordManualThrow(T(50));
    GameState win = m.snapshot();
    CHECK(win.gameOver==true);
    CHECK(win.winner.has_value() && *win.winner==0);
    CHECK(win.finishedPlayers.size()==2);
    m.undo();
    compare(s0, m.snapshot());
    // Play resumes: a further throw is now accepted (marks change again).
    m.recordManualThrow(T(50));
    CHECK(m.snapshot().gameOver==true);
}

// Undo after correctThrow reverts to pre-correction observable state.
static void undo_after_correct_throw(){
    GameManager m; newCricket(m,2);
    m.recordManualThrow(T(20));           // P0 dart1: marks[0]=1
    m.recordManualThrow(T(20));           // P0 dart2: marks[0]=2
    GameState before = m.snapshot();      // marks[0]=2, score 0
    // Locate P0 first dart (turn 0, throw 0) and correct it to a triple 20.
    m.correctThrow(0, 0, T(20,3));        // replay: 1+2 marks would be 4 -> close+1 overflow
    CHECK(m.snapshot().players[0].marks[0]==3);
    m.undo();
    compare(before, m.snapshot());
    CHECK(m.snapshot().players[0].marks[0]==2);
    CHECK(m.snapshot().players[0].score==0);
}

// Undo after nextPlayer restores the pre-handover state.
static void undo_after_next_player(){
    GameManager m; newCricket(m,2);
    m.recordManualThrow(T(20));           // P0 dart1
    GameState s0 = m.snapshot();
    m.nextPlayer();                       // force hand-over mid-turn
    m.undo();
    compare(s0, m.snapshot());
}

// Cut-throat: undo an overflow throw removes points dumped on opponents.
static void undo_cutthroat_overflow_removes_opp_points(){
    GameManager m; newCricket(m,3,/*cutThroat=*/true);
    m.recordManualThrow(T(20,3));         // P0 closes 20, no overflow
    GameState s0 = m.snapshot();
    CHECK(s0.players[0].marks[0]==3);
    CHECK(s0.players[1].score==0);
    CHECK(s0.players[2].score==0);
    m.recordManualThrow(T(20,3));         // 3 overflow -> +60 to each opponent (P1,P2)
    CHECK(m.snapshot().players[1].score==60);
    CHECK(m.snapshot().players[2].score==60);
    CHECK(m.snapshot().players[0].score==0);   // thrower gains nothing in cutthroat
    m.undo();
    compare(s0, m.snapshot());
    CHECK(m.snapshot().players[1].score==0);
    CHECK(m.snapshot().players[2].score==0);
}

// Undo a bull-closing throw (marks[6] revert).
static void undo_bull_close(){
    GameManager m; newCricket(m,2);
    m.recordManualThrow(T(25));           // bull: marks[6]=1
    m.recordManualThrow(T(25));           // marks[6]=2
    GameState s0 = m.snapshot();
    CHECK(s0.players[0].marks[6]==2);
    m.recordManualThrow(T(25));           // closes bull -> marks[6]=3
    CHECK(m.snapshot().players[0].marks[6]==3);
    m.undo();
    compare(s0, m.snapshot());
    CHECK(m.snapshot().players[0].marks[6]==2);
}

// Undo when useBull=false: throws on the bull are ignored; undo is still exact.
static void undo_nobull_ignored_throw(){
    GameManager m; newCricket(m,2,/*cutThroat=*/false,/*useBull=*/false);
    m.recordManualThrow(T(19,2));         // marks[1]=2
    GameState s0 = m.snapshot();
    m.recordManualThrow(T(25));           // ignored (bull not a target)
    m.undo();
    compare(s0, m.snapshot());
    CHECK(m.snapshot().players[0].marks[1]==2);
}

// Undo restores state after several opponent throws interleaved (2 players, standard).
static void undo_interleaved_players(){
    GameManager m; newCricket(m,2);
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(19)); m.recordManualThrow(T(19)); // P0 -> P1
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(18)); // P1 partial
    GameState s0 = m.snapshot();
    m.recordManualThrow(T(20,3));         // P1 dart3: overflow on 20 (P0 already closed 20? yes)
    // If both closed 20, no points; regardless, undo must revert exactly.
    m.undo();
    compare(s0, m.snapshot());
}

// Undo twice after two closing throws by different players.
static void undo_two_closes_two_players(){
    GameManager m; newCricket(m,2);
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(7)); m.recordManualThrow(T(7)); // P0 close 20 -> P1
    GameState s1 = m.snapshot();
    m.recordManualThrow(T(19,3));         // P1 close 19
    GameState s2 = m.snapshot();
    CHECK(s2.players[1].marks[1]==3);
    m.undo();
    compare(s1, m.snapshot());
    m.recordManualThrow(T(19,3));         // redo the close
    compare(s2, m.snapshot());
}

int main(){
    undo_single_throw_mid_turn();
    undo_closing_throw_reverts_marks();
    undo_overflow_points_reverts_score();
    undo_close_and_overflow_same_throw();
    undo_across_turn_boundary();
    multiple_sequential_undos_to_start();
    undo_winning_throw();
    undo_after_correct_throw();
    undo_after_next_player();
    undo_cutthroat_overflow_removes_opp_points();
    undo_bull_close();
    undo_nobull_ignored_throw();
    undo_interleaved_players();
    undo_two_closes_two_players();
    if(!g_failures) std::cout<<"all cricket_undo cases passed\n";
    else std::cerr<<g_failures<<" check(s) failed\n";
    return g_failures;
}
