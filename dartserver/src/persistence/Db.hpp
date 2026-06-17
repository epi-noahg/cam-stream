#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Db — SQLite persistence for players, finished matches and aggregate stats.
//
// The schema is a faithful subset of GoDartss' Prisma model (prisma/schema.prisma):
// player / game / game_participant / turn / throw / player_stats.  Badges are
// deferred (Phase 6).  A finished match is recorded atomically; per-player
// aggregates (games/wins/losses, averages, triples/doubles/bulls/180s) are
// updated in the same transaction.
//
// Thread-safety: a single mutex serializes access (sqlite handle is shared).
// ─────────────────────────────────────────────────────────────────────────────

#include "game/GameTypes.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace dart::persist {

struct PlayerRow {
    int         id {0};
    std::string nickname;
    int         totalGames {0};
    int         totalWins {0};
    int         totalLosses {0};
    std::optional<int> bestFinish;
};

struct LeaderboardRow {
    int         id {0};
    std::string nickname;
    int         totalGames {0};
    int         totalWins {0};
    double      winRate {0.0};
    double      averageScore {0.0};
};

struct SavedGameRow {
    std::string id;
    std::string players;       ///< comma-separated nicknames
    int         startingScore {0};
    std::string updatedAt;
};

struct GameSummary {
    int         id {0};
    std::string mode;
    std::string status;
    std::optional<int> winnerId;
    std::string winnerNickname;
    std::string players;        ///< participant nicknames, comma-separated
    int         startingScore {0};
    std::string finishedAt;
};

class Db {
public:
    Db() = default;
    ~Db();

    /// Open (creating if needed) the SQLite file and ensure the schema exists.
    bool open(const std::string& path);

    /// Find a player by nickname, creating them if absent.  Returns id or -1.
    int upsertPlayer(const std::string& nickname);

    /// Rename a player. False if the new nickname is already taken.
    bool renamePlayer(int id, const std::string& nickname);

    /// Delete a player (and their stats row). Historical games are kept.
    bool deletePlayer(int id);

    /// Persist a finished match (game + participants + turns + throws) and
    /// update per-player aggregates/stats.  Players are matched by nickname.
    /// Returns the new game id, or -1 on failure.
    int recordFinishedGame(const dart::game::GameState& s);

    // ── Reads (for the UI) ───────────────────────────────────────────────
    std::vector<PlayerRow>      listPlayers();
    std::vector<LeaderboardRow> leaderboard(int limit = 10);
    std::vector<GameSummary>    recentGames(int limit = 20);

    // ── Resumable in-progress games ──────────────────────────────────────
    /// Insert or update an in-progress game (state = serialized GameState JSON).
    void saveGame(const std::string& id, const std::string& stateJson,
                  const std::string& players, int startingScore);
    /// In-progress games, most recent first.
    std::vector<SavedGameRow> listSavedGames(int limit = 20);
    /// Serialized state JSON for a saved game, or empty.
    std::string loadGameState(const std::string& id);
    /// Drop a saved game (e.g. once finished).
    void deleteSavedGame(const std::string& id);

private:
    bool exec_(const std::string& sql);
    int  playerIdByNickname_(const std::string& nickname);

    sqlite3*           db_ {nullptr};
    mutable std::mutex mtx_;
};

} // namespace dart::persist
