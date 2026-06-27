// Tests for SQLite persistence: record a finished match, then verify player
// aggregates / stats / leaderboard.

#include "persistence/Db.hpp"
#include "game/GameManager.hpp"

#include <cstdio>
#include <iostream>

using namespace dart;

static int g_failures = 0;
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ++g_failures;                                                      \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  "        \
                      << #cond << "\n";                                        \
        }                                                                      \
    } while (0)

int main() {
    const std::string path = "/tmp/dartserver_test.db";
    std::remove(path.c_str());

    persist::Db db;
    CHECK(db.open(path));

    // Two players, 40 start, P0 wins on D20.
    game::GameManager gm;
    gm.setOnGameOver([&](const game::GameState& s) { db.recordFinishedGame(s); });
    game::OptionsX01 opts;
    opts.startingScore = 40;
    opts.outType = game::InOutType::Double;
    opts.legs = 1;
    gm.createGame({{0, "Alice", 40, 0, {}}, {1, "Bob", 40, 0, {}}}, opts);
    gm.recordManualThrow(game::Throw{20, 2, false});  // D20 → Alice wins, gameOver

    CHECK(gm.snapshot().gameOver);

    auto board = db.leaderboard(10);
    CHECK(board.size() == 2);
    // Alice should be on top with 1 win.
    bool aliceWon = false, bobLost = false;
    for (const auto& r : board) {
        if (r.nickname == "Alice") { aliceWon = (r.totalWins == 1 && r.totalGames == 1); }
        if (r.nickname == "Bob")   { bobLost  = (r.totalWins == 0 && r.totalGames == 1); }
    }
    CHECK(aliceWon);
    CHECK(bobLost);

    auto players = db.listPlayers();
    CHECK(players.size() == 2);

    auto games = db.recentGames(10);
    CHECK(games.size() == 1);
    CHECK(games[0].status == "FINISHED");

    // Resumable saved games: starting new matches while others are unfinished
    // must keep every one independently resumable (each under its own id).
    db.saveGame("gameA", R"({"mode":"X01"})", "Alice, Bob", 501);
    db.saveGame("gameB", R"({"mode":"CRICKET"})", "Carol, Dave", 0);
    auto saved = db.listSavedGames(20);
    CHECK(saved.size() == 2);
    CHECK(!db.loadGameState("gameA").empty());
    CHECK(!db.loadGameState("gameB").empty());

    // Finishing one removes only that one; the other stays resumable.
    db.deleteSavedGame("gameB");
    auto saved2 = db.listSavedGames(20);
    CHECK(saved2.size() == 1);
    CHECK(saved2[0].id == "gameA");
    CHECK(!db.loadGameState("gameA").empty());
    CHECK(db.loadGameState("gameB").empty());

    if (g_failures == 0) std::cout << "All persistence tests passed.\n";
    else std::cerr << g_failures << " check(s) failed.\n";
    return g_failures;
}
