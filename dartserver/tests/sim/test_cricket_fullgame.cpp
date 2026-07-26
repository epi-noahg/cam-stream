// Nasty-case end-to-end simulation tests for the authoritative dart engine.
// Long deterministic games with heavy interleaving of undo / correctThrow /
// nextPlayer, verifying GLOBAL END-STATE INVARIANTS (observable-only).
//
// These pin down CORRECT behaviour per the Cricket / RTC rules; some CHECKs are
// EXPECTED to fail against today's known-buggy engine.
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
static void newRTC(GameManager& m, int nplayers, std::vector<int> teamOf={}) {
    std::vector<PlayerState> ps;
    for (int i=0;i<nplayers;i++){ PlayerState p; p.id=i; p.nickname="P"+std::to_string(i);
        p.team = (i<(int)teamOf.size()?teamOf[i]:0); ps.push_back(p);}
    GameConfig cfg; cfg.mode=GameMode::RoundTheClock; cfg.roundClock.teams = teamOf.empty()?1:2;
    m.createGame(std::move(ps), cfg);
}

// ── Shared end-state invariant checker ───────────────────────────────────────
static const PlayerState* findPlayer(const GameState& gs, int id){
    for (const auto& p: gs.players) if (p.id==id) return &p;
    return nullptr;
}
// Two players are teammates only when they share a NON-ZERO team id (team 0 =
// solo: every solo player is their own team, hence an opponent to the others).
static bool teammates(const PlayerState& a, const PlayerState& b){
    return a.id!=b.id && a.team!=0 && a.team==b.team;
}

static void assertInvariants(const GameState& gs, bool cutThroat){
    // marks always in [0,3]; score never negative.
    for (const auto& p: gs.players){
        CHECK(p.score>=0);
        for (size_t i=0;i<p.marks.size();++i)
            CHECK(p.marks[i]>=0 && p.marks[i]<=3);
    }
    if (gs.gameOver){
        CHECK(gs.winner.has_value());
        // finishedPlayers contains every player id EXACTLY once (no dup/miss).
        CHECK(gs.finishedPlayers.size()==gs.players.size());
        for (const auto& p: gs.players){
            int c=0; for (int id: gs.finishedPlayers) if (id==p.id) ++c;
            CHECK(c==1);
        }
        if (gs.winner){
            int wid=*gs.winner;
            bool inFin=false; for (int id: gs.finishedPlayers) if (id==wid) inFin=true;
            CHECK(inFin);
            const PlayerState* w=findPlayer(gs,wid);
            CHECK(w!=nullptr);
            if (w){
                if (gs.mode==GameMode::Cricket){
                    int req = gs.cricket.useBull?7:6;
                    CHECK((int)w->marks.size()>=req);
                    for (int i=0;i<req && i<(int)w->marks.size();++i)
                        CHECK(w->marks[i]==3);           // winner closed all required
                    for (const auto& opp: gs.players){
                        if (opp.id==wid || teammates(*w,opp)) continue;
                        if (cutThroat) CHECK(w->score<=opp.score);  // lowest wins
                        else           CHECK(w->score>=opp.score);  // highest (lead or tie)
                    }
                } else if (gs.mode==GameMode::RoundTheClock){
                    CHECK(w->target==21);                // finished the clock
                }
            }
        }
    } else {
        CHECK(!gs.winner.has_value());
        CHECK(gs.finishedPlayers.empty());
    }
}

static void rec(GameManager& m, Throw t){ m.recordManualThrow(t); }

// Locate the last recorded dart (turnIndex,throwIndex) in the live turns.
static bool findLastThrow(const GameState& gs, int& ti, int& tj){
    ti=-1; tj=-1;
    for (int i=0;i<(int)gs.turns.size();++i)
        if (!gs.turns[i].empty()){ ti=i; tj=(int)gs.turns[i].size()-1; }
    return ti>=0;
}

// ── Scenario 1: standard 2p solo, clean sweep + banked overflow (hand-computed)
static void std2_clean_bank(){
    GameManager m; newCricket(m,2,/*cutThroat=*/false,/*useBull=*/true);
    // P0 turn1: close 20, then two triple-20 overflow -> bank 2*60 = 120.
    rec(m,T(20,3)); rec(m,T(20,3)); rec(m,T(20,3)); // auto handover -> P1
    m.nextPlayer();                                  // skip P1 -> back to P0
    rec(m,T(19,3)); rec(m,T(18,3)); rec(m,T(17,3));
    m.nextPlayer();
    rec(m,T(16,3)); rec(m,T(15,3)); rec(m,T(25));    // bull mark1
    m.nextPlayer();
    rec(m,T(25)); rec(m,T(25));                       // bull mark2,mark3 -> win

    GameState gs=m.snapshot();
    assertInvariants(gs,false);
    CHECK(gs.gameOver);
    CHECK(gs.winner==std::optional<int>(0));
    const PlayerState* p0=findPlayer(gs,0); const PlayerState* p1=findPlayer(gs,1);
    CHECK(p0 && p0->score==120);
    for (int i=0;i<7;i++) CHECK(p0->marks[i]==3);
    CHECK(p1 && p1->score==0);
    for (int i=0;i<7;i++) CHECK(p1->marks[i]==0);

    // Further throws after game over are a no-op.
    rec(m,T(20,3));
    GameState gs2=m.snapshot();
    CHECK(gs2.players[0].score==120);
    CHECK(gs2.gameOver && gs2.winner==std::optional<int>(0));
}

// ── Scenario 2: cut-throat 2p, points dumped on opponent (hand-computed) ─────
static void cutthroat2_dump(){
    GameManager m; newCricket(m,2,/*cutThroat=*/true,/*useBull=*/true);
    rec(m,T(20,3)); rec(m,T(20,3)); rec(m,T(20,3)); // close 20 + dump 120 onto P1
    m.nextPlayer();
    rec(m,T(19,3)); rec(m,T(18,3)); rec(m,T(17,3));
    m.nextPlayer();
    rec(m,T(16,3)); rec(m,T(15,3)); rec(m,T(25));
    m.nextPlayer();
    rec(m,T(25)); rec(m,T(25));                       // close bull -> win

    GameState gs=m.snapshot();
    assertInvariants(gs,true);
    CHECK(gs.gameOver);
    CHECK(gs.winner==std::optional<int>(0));
    const PlayerState* p0=findPlayer(gs,0); const PlayerState* p1=findPlayer(gs,1);
    CHECK(p0 && p0->score==0);
    for (int i=0;i<7;i++) CHECK(p0->marks[i]==3);
    CHECK(p1 && p1->score==120);
    for (int i=0;i<7;i++) CHECK(p1->marks[i]==0);
}

// ── Scenario 3: 3p standard, undo mid-turn + nextPlayer skips, invariants ────
static void std3_undo_skip(){
    GameManager m; newCricket(m,3,false,true);
    rec(m,T(20,3)); rec(m,T(20,3));                 // close20 + bank 60 (both opp open)
    rec(m,T(19,3));                                  // close19, handover -> P1
    m.undo();                                        // revert the T(19,3)
    rec(m,T(19,3));                                  // replay it, handover -> P1
    m.nextPlayer(); m.nextPlayer();                  // skip P1, P2 -> P0
    rec(m,T(18,3)); rec(m,T(17,3)); rec(m,T(16,3));
    m.nextPlayer(); m.nextPlayer();
    rec(m,T(15,3)); rec(m,T(25)); rec(m,T(25));      // bull mark2
    m.nextPlayer(); m.nextPlayer();
    rec(m,T(25));                                    // bull mark3 -> win

    GameState gs=m.snapshot();
    assertInvariants(gs,false);
    CHECK(gs.gameOver);
    CHECK(gs.winner==std::optional<int>(0));
    const PlayerState* p0=findPlayer(gs,0);
    CHECK(p0 && p0->score==60);
    for (int i=0;i<7;i++) CHECK(p0->marks[i]==3);
}

// ── Scenario 4: correctThrow rescoring a past dart (hand-computed) ───────────
static void correct_rescore(){
    GameManager m; newCricket(m,2,false,true);
    rec(m,T(20,3)); rec(m,T(20,3)); rec(m,T(20,3)); // 1 close + 2*overflow -> +120
    // Correct P0's FIRST dart of turn 0 into a miss -> only 1 overflow remains.
    m.correctThrow(0,0,T(1));
    GameState gs=m.snapshot();
    assertInvariants(gs,false);
    const PlayerState* p0=findPlayer(gs,0);
    // Now dart0 miss, dart1 closes 20, dart2 overflows -> bank 60.
    CHECK(p0 && p0->score==60);
    CHECK(p0->marks[0]==3);
    CHECK(!gs.gameOver);                              // 20 alone is not a win
}

// ── Scenario 5: win, correct winning dart to a miss (reopen), then refinish ──
static void reopen_and_refinish(){
    GameManager m; newCricket(m,2,false,true);
    rec(m,T(20,3)); rec(m,T(20,3)); rec(m,T(20,3));
    m.nextPlayer();
    rec(m,T(19,3)); rec(m,T(18,3)); rec(m,T(17,3));
    m.nextPlayer();
    rec(m,T(16,3)); rec(m,T(15,3)); rec(m,T(25));
    m.nextPlayer();
    rec(m,T(25)); rec(m,T(25));                       // win via bull mark3

    GameState won=m.snapshot();
    CHECK(won.gameOver && won.winner==std::optional<int>(0));
    int ti,tj; CHECK(findLastThrow(won,ti,tj));
    Throw winning=won.turns[ti][tj];
    CHECK(winning.value==25);

    // Correct the winning dart to a miss -> bull no longer closed -> reopen.
    m.correctThrow(ti,tj,T(1));
    GameState reopened=m.snapshot();
    assertInvariants(reopened,false);
    CHECK(!reopened.gameOver);
    CHECK(!reopened.winner.has_value());
    CHECK(reopened.finishedPlayers.empty());
    CHECK(reopened.players[0].marks[6]==2);          // bull back to 2 marks

    // Replay the identical winning dart -> refinish, same winner.
    m.correctThrow(ti,tj,winning);
    GameState refin=m.snapshot();
    assertInvariants(refin,false);
    CHECK(refin.gameOver);
    CHECK(refin.winner==std::optional<int>(0));
    CHECK(refin.players[0].score==won.players[0].score);
    for (int i=0;i<7;i++) CHECK(refin.players[0].marks[i]==3);
}

// ── Scenario 6: full Round the Clock game to a win ───────────────────────────
static void rtc_full(){
    GameManager m; newRTC(m,2);
    for (int v=1;v<=20;v++){
        rec(m,T(v));                 // hit current target -> advance
        if (v%3==0) m.nextPlayer();  // after 3 darts auto-handover; skip opp back to P0
    }
    GameState gs=m.snapshot();
    assertInvariants(gs,false);
    CHECK(gs.gameOver);
    CHECK(gs.winner==std::optional<int>(0));
    CHECK(gs.players[0].target==21);
    CHECK(gs.players[1].target==1);                  // opponent never advanced
    CHECK(gs.finishedPlayers.size()==2);

    // No further advancement after game over.
    rec(m,T(1));
    CHECK(m.snapshot().players[0].target==21);
}

// ── Scenario 7: mid-game, not finished -> winner nullopt, finished empty ─────
static void midgame_not_over(){
    GameManager m; newCricket(m,2,false,true);
    rec(m,T(20,3)); rec(m,T(19,3)); rec(m,T(18,3));
    m.nextPlayer();
    rec(m,T(7)); rec(m,T(13)); rec(m,T(14));          // opponent all non-targets (ignored)
    GameState gs=m.snapshot();
    assertInvariants(gs,false);
    CHECK(!gs.gameOver);
    CHECK(!gs.winner.has_value());
    CHECK(gs.finishedPlayers.empty());
    // Non-target values leave marks & score untouched.
    const PlayerState* p1=findPlayer(gs,1);
    CHECK(p1 && p1->score==0);
    for (int i=0;i<7;i++) CHECK(p1->marks[i]==0);
    CHECK(gs.players[0].marks[0]==3 && gs.players[0].marks[1]==3 && gs.players[0].marks[2]==3);
}

// ── Scenario 8: useBull=false — bull NOT required, win on 20..15 only ────────
static void nobull_win(){
    GameManager m; newCricket(m,2,/*cutThroat=*/false,/*useBull=*/false);
    rec(m,T(20,3)); rec(m,T(20,3)); rec(m,T(19,3)); // close20 + bank60, close19
    m.nextPlayer();
    rec(m,T(18,3)); rec(m,T(17,3)); rec(m,T(16,3));
    m.nextPlayer();
    rec(m,T(15,3));                                   // close15 -> all six closed -> win

    GameState gs=m.snapshot();
    assertInvariants(gs,false);
    CHECK(gs.gameOver);
    CHECK(gs.winner==std::optional<int>(0));
    const PlayerState* p0=findPlayer(gs,0);
    CHECK(p0 && p0->score==60);
    for (int i=0;i<6;i++) CHECK(p0->marks[i]==3);     // 20..15 closed
    CHECK(p0->marks[6]==0);                            // bull untouched, not needed
}

// ── Scenario 9: teams (2v2), teammates share marks/score, invariants ────────
static void teams_2v2(){
    // team1 = {P0,P2}, team2 = {P1,P3}
    GameManager m; newCricket(m,4,false,true,/*teamOf=*/{1,2,1,2});
    rec(m,T(20,3)); rec(m,T(20,3)); rec(m,T(19,3)); // team1: close20 +bank60, close19 -> P1
    m.nextPlayer();                                  // skip opponent P1 -> P2 (teammate)
    rec(m,T(18,3)); rec(m,T(17,3)); rec(m,T(16,3)); // P2 shares team1 marks -> P3
    m.nextPlayer();                                  // skip opponent P3 -> P0
    rec(m,T(15,3)); rec(m,T(25)); rec(m,T(25));      // close15, bull mark2 -> P1
    m.nextPlayer();                                  // skip P1 -> P2
    rec(m,T(25));                                    // bull mark3 -> team1 wins

    GameState gs=m.snapshot();
    assertInvariants(gs,/*cutThroat=*/false);
    CHECK(gs.gameOver);
    CHECK(gs.winner.has_value());
    // Winner is a team1 member.
    int wid=*gs.winner; CHECK(wid==0 || wid==2);
    // Teammates share marks and score identically.
    const PlayerState* p0=findPlayer(gs,0); const PlayerState* p2=findPlayer(gs,2);
    const PlayerState* p1=findPlayer(gs,1); const PlayerState* p3=findPlayer(gs,3);
    CHECK(p0 && p2 && p1 && p3);
    CHECK(p0->score==p2->score);
    CHECK(p0->marks==p2->marks);
    CHECK(p1->score==p3->score);
    CHECK(p1->marks==p3->marks);
    for (int i=0;i<7;i++) CHECK(p0->marks[i]==3);     // team1 closed everything
    CHECK(p0->score==60);
    CHECK(p1->score==0);
    // All four placed exactly once.
    CHECK(gs.finishedPlayers.size()==4);
}

// ── Scenario 10: heavy interleave — undo/correct/nextPlayer, then win ────────
static void heavy_interleave(){
    GameManager m; newCricket(m,3,false,true);
    // P0 opens, plays sloppily with corrections and undos.
    rec(m,T(20,3)); rec(m,T(20,3)); rec(m,T(20,3)); // close20 + bank 2*60=120 -> P1
    // Correct P0's 3rd dart (turn0,idx2) to a miss, then undo the correction.
    {
        GameState g=m.snapshot();
        // turn0 is P0's first visit with 3 darts.
        m.correctThrow(0,2,T(1));                    // now only 1 overflow -> score 60
        CHECK(m.snapshot().players[0].score==60);
        m.undo();                                    // revert correction -> back to 120
        CHECK(m.snapshot().players[0].score==120);
        (void)g;
    }
    m.nextPlayer(); m.nextPlayer();                  // skip P1,P2 -> P0
    rec(m,T(19,3)); rec(m,T(18,3));
    m.undo();                                        // drop the T(18,3)
    rec(m,T(18,3));                                  // replay it
    rec(m,T(17,3));                                  // handover -> P1
    m.nextPlayer(); m.nextPlayer();
    rec(m,T(16,3)); rec(m,T(15,3)); rec(m,T(25));    // bull mark1
    m.nextPlayer(); m.nextPlayer();
    rec(m,T(25)); rec(m,T(25));                       // bull mark3 -> win

    GameState gs=m.snapshot();
    assertInvariants(gs,false);
    CHECK(gs.gameOver);
    CHECK(gs.winner==std::optional<int>(0));
    const PlayerState* p0=findPlayer(gs,0);
    CHECK(p0 && p0->score==120);
    for (int i=0;i<7;i++) CHECK(p0->marks[i]==3);
    CHECK(gs.finishedPlayers.size()==3);
    // Post-game record is a no-op.
    rec(m,T(19,3));
    CHECK(m.snapshot().players[0].score==120);
}

int main(){
    std2_clean_bank();
    cutthroat2_dump();
    std3_undo_skip();
    correct_rescore();
    reopen_and_refinish();
    rtc_full();
    midgame_not_over();
    nobull_win();
    teams_2v2();
    heavy_interleave();
    if(!g_failures) std::cout<<"all fullgame cases passed\n";
    else std::cerr<<g_failures<<" check(s) failed\n";
    return g_failures;
}
