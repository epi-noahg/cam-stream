// Team Cricket nasty-case simulations (standard & cut-throat).
// 4 players, teamOf={1,2,1,2}: P0,P2 = team1 ; P1,P3 = team2.
// Rotation order by index: P0(t1), P1(t2), P2(t1), P3(t2).
// Asserts CORRECT rules (code is known-buggy; some CHECKs may fail).
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

static const std::vector<int> TEAMS = {1,2,1,2};

// True if the two players share identical marks(7) + score.
static void checkSameTeam(const GameState& gs, int a, int b, const char* tag){
    const auto& pa = gs.players[a]; const auto& pb = gs.players[b];
    if (pa.score != pb.score){ ++g_failures;
        std::cerr<<"FAIL score mismatch team ["<<tag<<"] p"<<a<<"="<<pa.score
                 <<" p"<<b<<"="<<pb.score<<"\n"; }
    bool marksOk = pa.marks.size()==7 && pb.marks.size()==7;
    for (int i=0;marksOk && i<7;i++) if (pa.marks[i]!=pb.marks[i]) marksOk=false;
    if (!marksOk){ ++g_failures;
        std::cerr<<"FAIL marks mismatch team ["<<tag<<"] p"<<a<<" vs p"<<b<<"\n"; }
}

// ── Cases ────────────────────────────────────────────────────────────────

// Teammates start identical and stay identical after a close.
static void case_share_after_close(){
    GameManager m; newCricket(m, 4, false, true, TEAMS);
    auto gs0 = m.snapshot();
    checkSameTeam(gs0, 0, 2, "init t1"); checkSameTeam(gs0, 1, 3, "init t2");

    m.recordManualThrow(T(20,3));                 // P0 closes 20 for team1
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[0] == 3);
    CHECK(gs.players[2].marks[0] == 3);           // teammate shares
    CHECK(gs.players[1].marks[0] == 0);           // opponents untouched
    CHECK(gs.players[3].marks[0] == 0);
    CHECK(gs.players[0].score == 0 && gs.players[2].score == 0);
    checkSameTeam(gs, 0, 2, "close t1");
}

// All 7 marks + score stay identical within each team after a mixed sequence.
static void case_share_all_marks(){
    GameManager m; newCricket(m, 4, false, true, TEAMS);
    m.recordManualThrow(T(20,3));                 // P0: close 20
    m.recordManualThrow(T(19,1));                 // P0: 1 mark on 19
    m.recordManualThrow(T(18,2));                 // P0: 2 marks on 18 (handover)
    m.recordManualThrow(T(20,3));                 // P1 (team2): close 20
    m.recordManualThrow(T(50));                   // P1: bull 2 marks
    m.recordManualThrow(T(3));                    // P1: junk (handover)
    auto gs = m.snapshot();
    checkSameTeam(gs, 0, 2, "mixed t1");
    checkSameTeam(gs, 1, 3, "mixed t2");
    CHECK(gs.players[2].marks[0]==3 && gs.players[2].marks[1]==1 && gs.players[2].marks[2]==2);
    CHECK(gs.players[3].marks[0]==3 && gs.players[3].marks[6]==2);
    // teams are distinct
    CHECK(gs.players[0].marks[6]==0);
}

// Standard overflow: team banks while OPPONENT team still has V open.
static void case_std_overflow_banks(){
    GameManager m; newCricket(m, 4, false, true, TEAMS);
    m.recordManualThrow(T(20,3));                 // P0 close 20 (0 overflow)
    m.recordManualThrow(T(20,3));                 // P0 3 overflow marks -> +60
    auto gs = m.snapshot();
    CHECK(gs.players[0].score == 60);
    CHECK(gs.players[2].score == 60);             // teammate shares banked points
    CHECK(gs.players[1].score == 0 && gs.players[3].score == 0);
}

// Close-and-overflow in the SAME throw (standard).
static void case_std_close_and_overflow(){
    GameManager m; newCricket(m, 4, false, true, TEAMS);
    m.recordManualThrow(T(20,1));                 // marks[0]=1
    m.recordManualThrow(T(20,1));                 // marks[0]=2
    m.recordManualThrow(T(20,3));                 // 1 closes, 2 overflow -> +40
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==3);
    CHECK(gs.players[0].score == 40 && gs.players[2].score == 40);
    CHECK(gs.players[1].score == 0 && gs.players[3].score == 0);
}

// Team-identity opponent detection (standard): opponent TEAM already closed V
// -> no banking, even though the thrower's teammate had it open.
static void case_std_opponent_team_closed_no_bank(){
    GameManager m; newCricket(m, 4, false, true, TEAMS);
    // P0 (team1) junk to hand over so P1 acts.
    m.recordManualThrow(T(3)); m.recordManualThrow(T(3)); m.recordManualThrow(T(3));
    // P1 (team2) closes 20 -> whole team2 closed 20.
    m.recordManualThrow(T(20,3));
    m.recordManualThrow(T(3)); m.recordManualThrow(T(3));   // handover
    // P2 (team1) closes 20 then overflows; teammate P0 still "had" 20 open a moment
    // ago but teammates are NOT opponents; the only opponent team (t2) closed it.
    m.recordManualThrow(T(20,3));                 // close 20 for team1
    m.recordManualThrow(T(20,3));                 // 3 overflow, opponents closed -> +0
    auto gs = m.snapshot();
    CHECK(gs.players[2].marks[0]==3 && gs.players[0].marks[0]==3);
    CHECK(gs.players[0].score == 0 && gs.players[2].score == 0);
    CHECK(gs.players[1].score == 0 && gs.players[3].score == 0);
}

// Cut-throat overflow dumps on BOTH opponent-team members, not teammates.
static void case_ct_overflow_dumps_opponent_team(){
    GameManager m; newCricket(m, 4, true, true, TEAMS);   // cutThroat
    m.recordManualThrow(T(20,3));                 // P0 close 20 (0 overflow)
    m.recordManualThrow(T(20,3));                 // 3 overflow -> +60 to opp team
    auto gs = m.snapshot();
    CHECK(gs.players[1].score == 60);             // opponent team both share
    CHECK(gs.players[3].score == 60);
    CHECK(gs.players[0].score == 0);              // thrower/team gain nothing
    CHECK(gs.players[2].score == 0);
    checkSameTeam(gs, 1, 3, "ct dump");
}

// Cut-throat: opponent team that already closed V is NOT charged.
static void case_ct_opponent_closed_not_charged(){
    GameManager m; newCricket(m, 4, true, true, TEAMS);
    // P0 junk handover.
    m.recordManualThrow(T(3)); m.recordManualThrow(T(3)); m.recordManualThrow(T(3));
    // P1 (team2) closes 20 -> team2 closed.
    m.recordManualThrow(T(20,3));
    m.recordManualThrow(T(3)); m.recordManualThrow(T(3));
    // P2 (team1) close+overflow; opponents already closed -> nobody charged.
    m.recordManualThrow(T(20,3));
    m.recordManualThrow(T(20,3));
    auto gs = m.snapshot();
    CHECK(gs.players[0].score==0 && gs.players[1].score==0
          && gs.players[2].score==0 && gs.players[3].score==0);
}

// Team finishes together (standard): closes all with exact triples, ties on
// points (0>=0) -> win. Both members finished, all placed, winner in team1.
static void case_std_team_finish(){
    GameManager m; newCricket(m, 4, false, true, TEAMS);
    // P0: close 20,19,18
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(19,3)); m.recordManualThrow(T(18,3));
    // P1: junk handover
    m.recordManualThrow(T(3)); m.recordManualThrow(T(3)); m.recordManualThrow(T(3));
    // P2 (team1): close 17,16,15
    m.recordManualThrow(T(17,3)); m.recordManualThrow(T(16,3)); m.recordManualThrow(T(15,3));
    // P3: junk handover
    m.recordManualThrow(T(3)); m.recordManualThrow(T(3)); m.recordManualThrow(T(3));
    // P0: bull 50 (2 marks) then 25 (1 mark) closes bull -> team1 all closed, win.
    m.recordManualThrow(T(50));
    m.recordManualThrow(T(25));
    auto gs = m.snapshot();
    CHECK(gs.gameOver);
    CHECK(gs.winner.has_value());
    if (gs.winner) CHECK(*gs.winner==0 || *gs.winner==2);
    CHECK(gs.finishedPlayers.size()==4);
    // both team1 members finished
    bool has0=false, has2=false;
    for (int id : gs.finishedPlayers){ if(id==0)has0=true; if(id==2)has2=true; }
    CHECK(has0 && has2);
    // record after gameOver is a no-op
    m.recordManualThrow(T(19,3));
    auto gs2 = m.snapshot();
    CHECK(gs2.gameOver && gs2.finishedPlayers.size()==4);
}

// Trailing team that closed everything but is BEHIND on points does not win
// (standard), then goes ahead and wins later. Uses cut-throat inverse? No —
// standard: team must be >= every opponent.
static void case_std_close_but_behind_no_win(){
    GameManager m; newCricket(m, 4, false, true, TEAMS);
    // P0 close 20, then bank a lead so team1 is AHEAD; then let team2 outscore.
    // Simpler: give team2 the lead first, then team1 closes all while behind.
    // P0 junk handover.
    m.recordManualThrow(T(3)); m.recordManualThrow(T(3)); m.recordManualThrow(T(3));
    // P1 (team2): close 20 then overflow +60 (team1 open) -> team2 score 60.
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(20,3)); m.recordManualThrow(T(20,3));
    auto gsMid = m.snapshot();
    CHECK(gsMid.players[1].score==120);           // 6 overflow marks *20
    CHECK(gsMid.players[3].score==120);
    // P2 (team1): close 19,18,17
    m.recordManualThrow(T(19,3)); m.recordManualThrow(T(18,3)); m.recordManualThrow(T(17,3));
    // P3 junk
    m.recordManualThrow(T(3)); m.recordManualThrow(T(3)); m.recordManualThrow(T(3));
    // P0 (team1): close 16,15, and bull(50)-> only 2 marks on bull (not closed)
    m.recordManualThrow(T(16,3)); m.recordManualThrow(T(15,3)); m.recordManualThrow(T(50));
    // team1 has 20 still open (never closed 20!). Not all closed -> not won.
    auto gs = m.snapshot();
    CHECK(!gs.gameOver);
    CHECK(gs.players[0].marks[0]==0);             // 20 never closed by team1
    CHECK(gs.players[0].score < gs.players[1].score); // behind
}

// correctThrow on a team member's past dart keeps both teammates consistent.
static void case_correct_keeps_team_consistent(){
    GameManager m; newCricket(m, 4, false, true, TEAMS);
    m.recordManualThrow(T(20,3));                 // P0 close 20 (turn0,throw0)
    auto before = m.snapshot();
    CHECK(before.players[0].marks[0]==3 && before.players[2].marks[0]==3);
    // Correct that first dart down to a single 20 -> only 1 mark for team1.
    m.correctThrow(0, 0, T(20,1));
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==1);
    CHECK(gs.players[2].marks[0]==1);             // teammate reflects correction
    CHECK(gs.players[0].score==0 && gs.players[2].score==0);
    checkSameTeam(gs, 0, 2, "corrected");
}

// correctThrow that removes an overflow re-derives shared score for both.
static void case_correct_removes_overflow(){
    GameManager m; newCricket(m, 4, false, true, TEAMS);
    m.recordManualThrow(T(20,3));                 // close 20
    m.recordManualThrow(T(20,3));                 // +60 overflow (turn0,throw1)
    auto mid = m.snapshot();
    CHECK(mid.players[0].score==60 && mid.players[2].score==60);
    m.correctThrow(0, 1, T(3));                   // change overflow dart to junk
    auto gs = m.snapshot();
    CHECK(gs.players[0].score==0 && gs.players[2].score==0);
    CHECK(gs.players[0].marks[0]==3 && gs.players[2].marks[0]==3);
    checkSameTeam(gs, 0, 2, "corr overflow");
}

// undo reverts BOTH teammates to the exact prior shared state.
static void case_undo_reverts_team(){
    GameManager m; newCricket(m, 4, false, true, TEAMS);
    m.recordManualThrow(T(20,3));                 // close 20
    auto snap1 = m.snapshot();
    CHECK(snap1.players[0].marks[0]==3 && snap1.players[2].marks[0]==3);
    m.recordManualThrow(T(20,3));                 // +60 overflow
    m.undo();                                     // revert overflow dart
    auto gs = m.snapshot();
    CHECK(gs.players[0].marks[0]==3 && gs.players[2].marks[0]==3);
    CHECK(gs.players[0].score==0 && gs.players[2].score==0);
    m.undo();                                     // revert the close
    auto gs2 = m.snapshot();
    CHECK(gs2.players[0].marks[0]==0 && gs2.players[2].marks[0]==0);
    checkSameTeam(gs2, 0, 2, "after undo");
}

// Cut-throat team finish: team1 closes all with no overflow (score 0),
// opponents 0, 0<=0 satisfies cut-throat goal -> team1 wins together.
static void case_ct_team_finish(){
    GameManager m; newCricket(m, 4, true, true, TEAMS);
    m.recordManualThrow(T(20,3)); m.recordManualThrow(T(19,3)); m.recordManualThrow(T(18,3));
    m.recordManualThrow(T(3)); m.recordManualThrow(T(3)); m.recordManualThrow(T(3)); // P1
    m.recordManualThrow(T(17,3)); m.recordManualThrow(T(16,3)); m.recordManualThrow(T(15,3)); // P2
    m.recordManualThrow(T(3)); m.recordManualThrow(T(3)); m.recordManualThrow(T(3)); // P3
    m.recordManualThrow(T(50)); m.recordManualThrow(T(25));                          // P0 bull
    auto gs = m.snapshot();
    CHECK(gs.gameOver);
    CHECK(gs.winner.has_value());
    if (gs.winner) CHECK(*gs.winner==0 || *gs.winner==2);
    CHECK(gs.finishedPlayers.size()==4);
    CHECK(gs.players[0].score==0 && gs.players[2].score==0);
}

int main(){
    case_share_after_close();
    case_share_all_marks();
    case_std_overflow_banks();
    case_std_close_and_overflow();
    case_std_opponent_team_closed_no_bank();
    case_ct_overflow_dumps_opponent_team();
    case_ct_opponent_closed_not_charged();
    case_std_team_finish();
    case_std_close_but_behind_no_win();
    case_correct_keeps_team_consistent();
    case_correct_removes_overflow();
    case_undo_reverts_team();
    case_ct_team_finish();
    if(!g_failures) std::cout<<"all cricket_teams cases passed\n";
    else std::cerr<<g_failures<<" check(s) failed\n";
    return g_failures;
}
