#include "Db.hpp"

#include <sqlite3.h>

#include <iostream>

namespace dart::persist {
namespace {

const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS player (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    nickname     TEXT UNIQUE NOT NULL,
    totalGames   INTEGER NOT NULL DEFAULT 0,
    totalWins    INTEGER NOT NULL DEFAULT 0,
    totalLosses  INTEGER NOT NULL DEFAULT 0,
    bestFinish   INTEGER,
    createdAt    TEXT NOT NULL DEFAULT (datetime('now')),
    lastActiveAt TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS game (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    mode          TEXT NOT NULL DEFAULT 'X01',
    status        TEXT NOT NULL DEFAULT 'FINISHED',
    startingScore INTEGER,
    doubleOut     INTEGER NOT NULL DEFAULT 0,
    doubleIn      INTEGER NOT NULL DEFAULT 0,
    winnerId      INTEGER,
    winnerRank    TEXT,
    createdAt     TEXT NOT NULL DEFAULT (datetime('now')),
    finishedAt    TEXT
);
CREATE TABLE IF NOT EXISTS game_participant (
    id       INTEGER PRIMARY KEY AUTOINCREMENT,
    gameId   INTEGER NOT NULL,
    playerId INTEGER NOT NULL,
    team     INTEGER NOT NULL DEFAULT 1,
    ord      INTEGER NOT NULL,
    UNIQUE(gameId, playerId)
);
CREATE TABLE IF NOT EXISTS turn (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    gameId        INTEGER NOT NULL,
    participantId INTEGER NOT NULL,
    orderInGame   INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS throw (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    turnId     INTEGER NOT NULL,
    value      INTEGER NOT NULL,
    multiplier INTEGER NOT NULL DEFAULT 1,
    bust       INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS player_stats (
    playerId     INTEGER PRIMARY KEY,
    averageScore REAL NOT NULL DEFAULT 0,
    totalThrows  INTEGER NOT NULL DEFAULT 0,
    totalScore   INTEGER NOT NULL DEFAULT 0,
    bull50Count  INTEGER NOT NULL DEFAULT 0,
    bull25Count  INTEGER NOT NULL DEFAULT 0,
    tripleCount  INTEGER NOT NULL DEFAULT 0,
    doubleCount  INTEGER NOT NULL DEFAULT 0,
    total180s    INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS saved_game (
    id            TEXT PRIMARY KEY,
    state         TEXT NOT NULL,
    players       TEXT,
    startingScore INTEGER,
    updatedAt     TEXT NOT NULL DEFAULT (datetime('now'))
);
)SQL";

} // namespace

Db::~Db() {
    if (db_) sqlite3_close(db_);
}

bool Db::exec_(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[db] error: " << (err ? err : "?") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Db::open(const std::string& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "[db] cannot open " << path << ": " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    exec_("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL;");
    return exec_(kSchema);
}

int Db::playerIdByNickname_(const std::string& nickname) {
    sqlite3_stmt* st = nullptr;
    int id = -1;
    if (sqlite3_prepare_v2(db_, "SELECT id FROM player WHERE nickname=?;", -1,
                           &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, nickname.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) id = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    return id;
}

int Db::upsertPlayer(const std::string& nickname) {
    std::lock_guard<std::mutex> lk(mtx_);
    int id = playerIdByNickname_(nickname);
    if (id >= 0) return id;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "INSERT INTO player(nickname) VALUES(?);", -1,
                           &st, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, nickname.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return -1;
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

bool Db::renamePlayer(int id, const std::string& nickname) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!db_ || nickname.empty()) return false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE player SET nickname=? WHERE id=?;", -1,
                           &st, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, nickname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, id);
    const int rc = sqlite3_step(st);  // UNIQUE conflict -> not SQLITE_DONE
    sqlite3_finalize(st);
    return rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

bool Db::deletePlayer(int id) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!db_) return false;
    exec_("DELETE FROM player_stats WHERE playerId=" + std::to_string(id) + ";");
    return exec_("DELETE FROM player WHERE id=" + std::to_string(id) + ";");
}

int Db::recordFinishedGame(const dart::game::GameState& s) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!db_ || s.players.empty()) return -1;

    exec_("BEGIN;");

    // Map GameState player ids → DB player ids (by nickname).
    auto dbIdFor = [&](int statePlayerId) -> int {
        for (const auto& p : s.players)
            if (p.id == statePlayerId) {
                int id = playerIdByNickname_(p.nickname);
                if (id < 0) {
                    sqlite3_stmt* st = nullptr;
                    sqlite3_prepare_v2(db_, "INSERT INTO player(nickname) VALUES(?);",
                                       -1, &st, nullptr);
                    sqlite3_bind_text(st, 1, p.nickname.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(st);
                    sqlite3_finalize(st);
                    id = static_cast<int>(sqlite3_last_insert_rowid(db_));
                }
                return id;
            }
        return -1;
    };

    // ── game row ─────────────────────────────────────────────────────────
    const int dbWinner = s.winner.has_value() ? dbIdFor(*s.winner) : -1;
    {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT INTO game(mode,status,startingScore,doubleOut,doubleIn,"
            "winnerId,finishedAt) VALUES('X01','FINISHED',?,?,?,?,datetime('now'));",
            -1, &st, nullptr);
        sqlite3_bind_int(st, 1, s.options.startingScore);
        sqlite3_bind_int(st, 2, s.options.outType == game::InOutType::Double ? 1 : 0);
        sqlite3_bind_int(st, 3, s.options.inType  == game::InOutType::Double ? 1 : 0);
        if (dbWinner >= 0) sqlite3_bind_int(st, 4, dbWinner);
        else               sqlite3_bind_null(st, 4);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    const int gameId = static_cast<int>(sqlite3_last_insert_rowid(db_));

    // ── participants (in player order) ───────────────────────────────────
    std::vector<int> participantId(s.players.size(), -1);
    std::vector<int> dbPlayerId(s.players.size(), -1);
    for (std::size_t i = 0; i < s.players.size(); ++i) {
        const int pid = dbIdFor(s.players[i].id);
        dbPlayerId[i] = pid;
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT INTO game_participant(gameId,playerId,team,ord) VALUES(?,?,1,?);",
            -1, &st, nullptr);
        sqlite3_bind_int(st, 1, gameId);
        sqlite3_bind_int(st, 2, pid);
        sqlite3_bind_int(st, 3, static_cast<int>(i));
        sqlite3_step(st);
        sqlite3_finalize(st);
        participantId[i] = static_cast<int>(sqlite3_last_insert_rowid(db_));
    }

    // ── turns + throws (turn t belongs to player t % nplayers) ───────────
    const int nplayers = static_cast<int>(s.players.size());
    for (std::size_t t = 0; t < s.turns.size(); ++t) {
        if (s.turns[t].empty()) continue;
        const int pIdx = static_cast<int>(t) % nplayers;
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT INTO turn(gameId,participantId,orderInGame) VALUES(?,?,?);",
            -1, &st, nullptr);
        sqlite3_bind_int(st, 1, gameId);
        sqlite3_bind_int(st, 2, participantId[pIdx]);
        sqlite3_bind_int(st, 3, static_cast<int>(t));
        sqlite3_step(st);
        sqlite3_finalize(st);
        const int turnId = static_cast<int>(sqlite3_last_insert_rowid(db_));

        for (const auto& thr : s.turns[t]) {
            sqlite3_stmt* ts = nullptr;
            sqlite3_prepare_v2(db_,
                "INSERT INTO throw(turnId,value,multiplier,bust) VALUES(?,?,?,?);",
                -1, &ts, nullptr);
            sqlite3_bind_int(ts, 1, turnId);
            sqlite3_bind_int(ts, 2, thr.value);
            sqlite3_bind_int(ts, 3, thr.multiplier);
            sqlite3_bind_int(ts, 4, thr.bust ? 1 : 0);
            sqlite3_step(ts);
            sqlite3_finalize(ts);
        }
    }

    // ── aggregates + stats per player ────────────────────────────────────
    for (std::size_t i = 0; i < s.players.size(); ++i) {
        const auto& p = s.players[i];
        const int pid = dbPlayerId[i];
        const bool isWinner = s.winner.has_value() && *s.winner == p.id;

        int throws = 0, score = 0, triples = 0, doubles = 0, bull50 = 0, bull25 = 0;
        for (const auto& thr : p.throws) {
            ++throws;
            if (!thr.bust) score += thr.hitValue();
            if (thr.multiplier == 3) ++triples;
            if (thr.multiplier == 2) ++doubles;
            if (thr.value == 50) ++bull50;
            if (thr.value == 25) ++bull25;
        }
        // 180s: this player's turns that sum to 180.
        int s180 = 0;
        for (std::size_t t = i; t < s.turns.size(); t += nplayers) {
            int sum = 0; bool bust = false;
            for (const auto& thr : s.turns[t]) {
                if (thr.bust) bust = true;
                sum += thr.hitValue();
            }
            if (!bust && sum == 180) ++s180;
        }

        exec_(std::string("UPDATE player SET totalGames=totalGames+1, ")
              + (isWinner ? "totalWins=totalWins+1, " : "totalLosses=totalLosses+1, ")
              + "lastActiveAt=datetime('now') WHERE id=" + std::to_string(pid) + ";");

        // Upsert stats (accumulate).
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT INTO player_stats(playerId,totalThrows,totalScore,tripleCount,"
            "doubleCount,bull50Count,bull25Count,total180s,averageScore) "
            "VALUES(?,?,?,?,?,?,?,?,?) "
            "ON CONFLICT(playerId) DO UPDATE SET "
            "totalThrows=totalThrows+excluded.totalThrows, "
            "totalScore=totalScore+excluded.totalScore, "
            "tripleCount=tripleCount+excluded.tripleCount, "
            "doubleCount=doubleCount+excluded.doubleCount, "
            "bull50Count=bull50Count+excluded.bull50Count, "
            "bull25Count=bull25Count+excluded.bull25Count, "
            "total180s=total180s+excluded.total180s, "
            "averageScore=CASE WHEN (totalThrows)>0 THEN "
            "  CAST(totalScore AS REAL)/(totalThrows) ELSE 0 END;",
            -1, &st, nullptr);
        sqlite3_bind_int(st, 1, pid);
        sqlite3_bind_int(st, 2, throws);
        sqlite3_bind_int(st, 3, score);
        sqlite3_bind_int(st, 4, triples);
        sqlite3_bind_int(st, 5, doubles);
        sqlite3_bind_int(st, 6, bull50);
        sqlite3_bind_int(st, 7, bull25);
        sqlite3_bind_int(st, 8, s180);
        sqlite3_bind_double(st, 9, throws > 0 ? double(score) / throws : 0.0);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }

    exec_("COMMIT;");
    return gameId;
}

void Db::saveGame(const std::string& id, const std::string& stateJson,
                  const std::string& players, int startingScore) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!db_) return;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
        "INSERT INTO saved_game(id,state,players,startingScore,updatedAt) "
        "VALUES(?,?,?,?,datetime('now')) "
        "ON CONFLICT(id) DO UPDATE SET state=excluded.state, "
        "players=excluded.players, startingScore=excluded.startingScore, "
        "updatedAt=datetime('now');", -1, &st, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, stateJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, players.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 4, startingScore);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

std::vector<SavedGameRow> Db::listSavedGames(int limit) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<SavedGameRow> out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
        "SELECT id,COALESCE(players,''),COALESCE(startingScore,0),updatedAt "
        "FROM saved_game ORDER BY updatedAt DESC LIMIT ?;", -1, &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, limit);
        while (sqlite3_step(st) == SQLITE_ROW) {
            SavedGameRow r;
            r.id            = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
            r.players       = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
            r.startingScore = sqlite3_column_int(st, 2);
            r.updatedAt     = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
            out.push_back(std::move(r));
        }
    }
    sqlite3_finalize(st);
    return out;
}

std::string Db::loadGameState(const std::string& id) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::string out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT state FROM saved_game WHERE id=?;", -1,
                           &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW)
            out = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
    }
    sqlite3_finalize(st);
    return out;
}

void Db::deleteSavedGame(const std::string& id) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!db_) return;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM saved_game WHERE id=?;", -1,
                           &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
}

std::vector<PlayerRow> Db::listPlayers() {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<PlayerRow> out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
        "SELECT id,nickname,totalGames,totalWins,totalLosses,bestFinish "
        "FROM player ORDER BY nickname;", -1, &st, nullptr) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            PlayerRow r;
            r.id          = sqlite3_column_int(st, 0);
            r.nickname    = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
            r.totalGames  = sqlite3_column_int(st, 2);
            r.totalWins   = sqlite3_column_int(st, 3);
            r.totalLosses = sqlite3_column_int(st, 4);
            if (sqlite3_column_type(st, 5) != SQLITE_NULL)
                r.bestFinish = sqlite3_column_int(st, 5);
            out.push_back(std::move(r));
        }
    }
    sqlite3_finalize(st);
    return out;
}

std::vector<LeaderboardRow> Db::leaderboard(int limit) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<LeaderboardRow> out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
        "SELECT p.id,p.nickname,p.totalGames,p.totalWins,"
        "       COALESCE(s.averageScore,0) "
        "FROM player p LEFT JOIN player_stats s ON s.playerId=p.id "
        "ORDER BY p.totalWins DESC, p.totalGames DESC LIMIT ?;",
        -1, &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, limit);
        while (sqlite3_step(st) == SQLITE_ROW) {
            LeaderboardRow r;
            r.id           = sqlite3_column_int(st, 0);
            r.nickname     = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
            r.totalGames   = sqlite3_column_int(st, 2);
            r.totalWins    = sqlite3_column_int(st, 3);
            r.averageScore = sqlite3_column_double(st, 4);
            r.winRate      = r.totalGames > 0 ? double(r.totalWins) / r.totalGames : 0.0;
            out.push_back(std::move(r));
        }
    }
    sqlite3_finalize(st);
    return out;
}

std::vector<GameSummary> Db::recentGames(int limit) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<GameSummary> out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_,
        "SELECT g.id, g.mode, g.status, g.winnerId, COALESCE(g.finishedAt,''), "
        "       COALESCE(w.nickname,''), COALESCE(g.startingScore,0), "
        "       (SELECT group_concat(p.nickname, ', ') "
        "          FROM game_participant gp JOIN player p ON p.id=gp.playerId "
        "         WHERE gp.gameId=g.id) "
        "FROM game g LEFT JOIN player w ON w.id=g.winnerId "
        "ORDER BY g.id DESC LIMIT ?;", -1, &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, limit);
        while (sqlite3_step(st) == SQLITE_ROW) {
            GameSummary g;
            g.id     = sqlite3_column_int(st, 0);
            g.mode   = reinterpret_cast<const char*>(sqlite3_column_text(st, 1));
            g.status = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
            if (sqlite3_column_type(st, 3) != SQLITE_NULL)
                g.winnerId = sqlite3_column_int(st, 3);
            g.finishedAt     = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
            g.winnerNickname = reinterpret_cast<const char*>(sqlite3_column_text(st, 5));
            g.startingScore  = sqlite3_column_int(st, 6);
            if (sqlite3_column_type(st, 7) != SQLITE_NULL)
                g.players = reinterpret_cast<const char*>(sqlite3_column_text(st, 7));
            out.push_back(std::move(g));
        }
    }
    sqlite3_finalize(st);
    return out;
}

} // namespace dart::persist
