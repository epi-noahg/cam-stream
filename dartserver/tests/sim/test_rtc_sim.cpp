// Round the Clock nasty-case simulation tests.
// Asserts AUTHORITATIVE RTC rules (may fail against the current buggy engine).
#include "game/GameManager.hpp"
#include <iostream>
#include <string>
#include <vector>
using namespace dart::game;
static int g_failures = 0;
#define CHECK(cond) do { if(!(cond)){ ++g_failures; \
  std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<"  "<<#cond<<"\n"; } } while(0)
static Throw T(int v, int mult=1){ return Throw{v, mult, false}; }

static void newRTC(GameManager& m, int nplayers, std::vector<int> teamOf={}) {
    std::vector<PlayerState> ps;
    for (int i=0;i<nplayers;i++){ PlayerState p; p.id=i; p.nickname="P"+std::to_string(i);
        p.team = (i<(int)teamOf.size()?teamOf[i]:0); ps.push_back(p);}
    GameConfig cfg; cfg.mode=GameMode::RoundTheClock; cfg.roundClock.teams = teamOf.empty()?1:2;
    m.createGame(std::move(ps), cfg);
}

// ── local read helpers (observable state only) ──────────────────────────────
static const PlayerState* P(const GameState& gs, int id){
    for (const auto& p : gs.players) if (p.id==id) return &p;
    return nullptr;
}
static bool inFinished(const GameState& gs, int id){
    for (int f : gs.finishedPlayers) if (f==id) return true;
    return false;
}
// Locate the first throw with the given value; used ONLY to drive correctThrow.
static bool locate(const GameState& gs, int value, int& ti, int& tj){
    for (int i=0;i<(int)gs.turns.size();++i)
        for (int j=0;j<(int)gs.turns[i].size();++j)
            if (gs.turns[i][j].value==value){ ti=i; tj=j; return true; }
    return false;
}
static bool locateLast(const GameState& gs, int value, int& ti, int& tj){
    bool found=false;
    for (int i=0;i<(int)gs.turns.size();++i)
        for (int j=0;j<(int)gs.turns[i].size();++j)
            if (gs.turns[i][j].value==value){ ti=i; tj=j; found=true; }
    return found;
}

// ── cases ───────────────────────────────────────────────────────────────────

static void rtc_target_starts_at_one(){
    GameManager m; newRTC(m,1);
    auto gs=m.snapshot();
    CHECK(P(gs,0)->target==1);
    CHECK(!gs.gameOver);
    CHECK(!gs.winner.has_value());
    CHECK(gs.finishedPlayers.empty());
}

static void rtc_single_advances(){
    GameManager m; newRTC(m,1);
    m.recordManualThrow(T(1));
    CHECK(P(m.snapshot(),0)->target==2);
}

static void rtc_double_advances_by_one(){
    GameManager m; newRTC(m,1);
    m.recordManualThrow(T(1,2));               // double 1
    CHECK(P(m.snapshot(),0)->target==2);       // multiplier irrelevant
}

static void rtc_triple_advances_by_one(){
    GameManager m; newRTC(m,1);
    m.recordManualThrow(T(1,3));               // triple 1
    CHECK(P(m.snapshot(),0)->target==2);       // advances by exactly one
}

static void rtc_nontarget_no_advance(){
    GameManager m; newRTC(m,1);
    m.recordManualThrow(T(5));                  // target is 1 -> miss
    CHECK(P(m.snapshot(),0)->target==1);
    m.recordManualThrow(T(1));                  // hit -> 2
    CHECK(P(m.snapshot(),0)->target==2);
    m.recordManualThrow(T(5));                  // target 2 -> miss again
    CHECK(P(m.snapshot(),0)->target==2);
    m.recordManualThrow(T(2));                  // hit -> 3
    CHECK(P(m.snapshot(),0)->target==3);
}

static void rtc_full_solo_run_finishes(){
    GameManager m; newRTC(m,1);
    for (int v=1; v<=20; ++v) m.recordManualThrow(T(v));
    auto gs=m.snapshot();
    CHECK(P(gs,0)->target==21);
    CHECK(gs.gameOver);
    CHECK(gs.winner.has_value() && *gs.winner==0);
    CHECK(inFinished(gs,0));
    CHECK(gs.finishedPlayers.size()==1);
    // Further throws are a no-op once gameOver.
    m.recordManualThrow(T(1));
    CHECK(P(m.snapshot(),0)->target==21);
}

static void rtc_two_player_race_first_finishes(){
    GameManager m; newRTC(m,2);
    // P0 hits three targets per visit; P1 always misses (target stays 1).
    // P0 visits cover [1..3],[4..6],...,[19,20]; finishes on the 20 hit.
    const int p0[7][3] = {{1,2,3},{4,5,6},{7,8,9},{10,11,12},
                          {13,14,15},{16,17,18},{19,20,0}};
    for (int v=0; v<7; ++v){
        for (int d=0; d<3; ++d){
            if (p0[v][d]==0) break;
            if (m.snapshot().gameOver) break;
            m.recordManualThrow(T(p0[v][d]));
        }
        if (m.snapshot().gameOver) break;
        for (int d=0; d<3; ++d) m.recordManualThrow(T(11)); // P1 misses
    }
    auto gs=m.snapshot();
    CHECK(gs.gameOver);
    CHECK(gs.winner.has_value() && *gs.winner==0);
    // RTC ends immediately: BOTH players placed once the first finishes.
    CHECK(inFinished(gs,0));
    CHECK(inFinished(gs,1));
    CHECK(gs.finishedPlayers.size()==2);
    // Loser made no progress.
    CHECK(P(gs,1)->target==1);
}

static void rtc_undo_advance_reverts_target(){
    GameManager m; newRTC(m,1);
    m.recordManualThrow(T(1));
    CHECK(P(m.snapshot(),0)->target==2);
    m.undo();
    CHECK(P(m.snapshot(),0)->target==1);
}

static void rtc_undo_winning_hit_reopens(){
    GameManager m; newRTC(m,1);
    for (int v=1; v<=19; ++v) m.recordManualThrow(T(v));
    CHECK(P(m.snapshot(),0)->target==20);
    m.recordManualThrow(T(20));                 // winning hit
    {
        auto gs=m.snapshot();
        CHECK(gs.gameOver && gs.winner.has_value() && *gs.winner==0);
    }
    m.undo();                                    // reopen
    auto gs=m.snapshot();
    CHECK(!gs.gameOver);
    CHECK(!gs.winner.has_value());
    CHECK(gs.finishedPlayers.empty());
    CHECK(P(gs,0)->target==20);
}

static void rtc_correct_past_hit_to_miss_drops_target(){
    GameManager m; newRTC(m,1);
    m.recordManualThrow(T(1));                   // ->2
    m.recordManualThrow(T(2));                   // ->3
    m.recordManualThrow(T(3));                   // ->4
    CHECK(P(m.snapshot(),0)->target==4);
    int ti,tj; CHECK(locate(m.snapshot(),1,ti,tj));
    m.correctThrow(ti,tj,T(15));                 // first hit becomes a miss
    // Replay: 15(miss),2(miss@1),3(miss@1) -> never advances.
    CHECK(P(m.snapshot(),0)->target==1);
}

static void rtc_correct_past_miss_to_hit_raises_target(){
    GameManager m; newRTC(m,1);
    m.recordManualThrow(T(9));                   // miss @1
    m.recordManualThrow(T(2));                   // miss @1
    m.recordManualThrow(T(3));                   // miss @1
    CHECK(P(m.snapshot(),0)->target==1);
    int ti,tj; CHECK(locate(m.snapshot(),9,ti,tj));
    m.correctThrow(ti,tj,T(1));                  // first miss becomes a hit
    // Replay: 1->2, 2->3, 3->4.
    CHECK(P(m.snapshot(),0)->target==4);
}

static void rtc_nextplayer_skip_then_correct_no_leak(){
    GameManager m; newRTC(m,2);
    m.recordManualThrow(T(1));                   // P0 hits -> P0.target 2
    m.nextPlayer();                              // skip rest of P0's darts
    m.recordManualThrow(T(1));                   // P1 -> 2
    m.recordManualThrow(T(2));                   // P1 -> 3
    m.recordManualThrow(T(3));                   // P1 -> 4
    {
        auto gs=m.snapshot();
        CHECK(P(gs,0)->target==2);
        CHECK(P(gs,1)->target==4);
    }
    // Correct P0's single dart into a miss; attribution must stay with P0.
    int ti,tj; CHECK(locate(m.snapshot(),1,ti,tj)); // P0's dart is the first '1'
    m.correctThrow(ti,tj,T(20));
    auto gs=m.snapshot();
    CHECK(P(gs,0)->target==1);                   // P0 lost its advance
    CHECK(P(gs,1)->target==4);                   // NO leakage onto P1
}

static void rtc_teams_share_target(){
    GameManager m; newRTC(m,4,{1,1,2,2});
    m.recordManualThrow(T(1));                   // P0 (team1) hits
    auto gs=m.snapshot();
    CHECK(P(gs,0)->target==2);
    CHECK(P(gs,1)->target==2);                   // teammate shares target
    CHECK(P(gs,2)->target==1);                   // opponents unaffected
    CHECK(P(gs,3)->target==1);
}

static void rtc_team_finishes_together(){
    GameManager m; newRTC(m,4,{1,1,2,2});
    int guard=0;
    for (;;){
        auto gs=m.snapshot();
        if (gs.gameOver) break;
        if (++guard>600) break;                  // safety
        const auto& cur=gs.players[gs.currentIndex];
        if (cur.team==1) m.recordManualThrow(T(cur.target)); // team1 advances
        else             m.recordManualThrow(T(19));         // team2 misses (target<20)
    }
    auto gs=m.snapshot();
    CHECK(gs.gameOver);
    CHECK(gs.winner.has_value());
    CHECK(*gs.winner==0 || *gs.winner==1);       // a team1 member won
    CHECK(inFinished(gs,0) && inFinished(gs,1));  // whole team finishes together
    CHECK(inFinished(gs,2) && inFinished(gs,3));  // all placed on immediate end
    CHECK(gs.finishedPlayers.size()==4);
    // Shared team state stays identical.
    CHECK(P(gs,0)->target==P(gs,1)->target);
}

static void rtc_correct_finished_game_stays_finished(){
    GameManager m; newRTC(m,1);
    for (int v=1; v<=20; ++v) m.recordManualThrow(T(v));
    CHECK(m.snapshot().gameOver);
    int ti,tj; CHECK(locate(m.snapshot(),1,ti,tj));
    m.correctThrow(ti,tj,T(1));                  // re-affirm first hit; still a full run
    auto gs=m.snapshot();
    CHECK(gs.gameOver);
    CHECK(gs.winner.has_value() && *gs.winner==0);
    CHECK(P(gs,0)->target==21);
    CHECK(inFinished(gs,0));
}

static void rtc_correct_finished_game_reopens_on_miss(){
    GameManager m; newRTC(m,1);
    for (int v=1; v<=20; ++v) m.recordManualThrow(T(v));
    CHECK(m.snapshot().gameOver);
    int ti,tj; CHECK(locateLast(m.snapshot(),20,ti,tj)); // the winning dart
    m.correctThrow(ti,tj,T(5));                  // winning hit becomes a miss
    auto gs=m.snapshot();
    CHECK(!gs.gameOver);
    CHECK(!gs.winner.has_value());
    CHECK(gs.finishedPlayers.empty());
    CHECK(P(gs,0)->target==20);                  // stalled one short
}

int main(){
    rtc_target_starts_at_one();
    rtc_single_advances();
    rtc_double_advances_by_one();
    rtc_triple_advances_by_one();
    rtc_nontarget_no_advance();
    rtc_full_solo_run_finishes();
    rtc_two_player_race_first_finishes();
    rtc_undo_advance_reverts_target();
    rtc_undo_winning_hit_reopens();
    rtc_correct_past_hit_to_miss_drops_target();
    rtc_correct_past_miss_to_hit_raises_target();
    rtc_nextplayer_skip_then_correct_no_leak();
    rtc_teams_share_target();
    rtc_team_finishes_together();
    rtc_correct_finished_game_stays_finished();
    rtc_correct_finished_game_reopens_on_miss();
    if(!g_failures) std::cout<<"all rtc_sim cases passed\n";
    else std::cerr<<g_failures<<" check(s) failed\n";
    return g_failures;
}
