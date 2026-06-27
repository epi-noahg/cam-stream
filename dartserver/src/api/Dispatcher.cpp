#include "Dispatcher.hpp"

#include "Json.hpp"
#include "detection/DetectionService.hpp"
#include "game/GameManager.hpp"
#include "persistence/Db.hpp"

namespace dart::api {

Dispatcher::Dispatcher(WsServer& ws, dart::game::GameManager& gm,
                       dart::detect::DetectionService& det,
                       dart::persist::Db* db)
    : ws_(ws), gm_(gm), det_(det), db_(db) {}

void Dispatcher::wire() {
    // ── Outbound: authoritative state → all clients ─────────────────────
    gm_.setOnChanged([this](const dart::game::GameState& s) {
        ws_.broadcast(gameStateToJson(s, gm_.currentCheckout(), gm_.gameId()).dump());
        // Persist the in-progress game so it can be resumed later.
        if (db_ && !s.players.empty()) {
            std::string players;
            for (const auto& p : s.players)
                players += (players.empty() ? "" : ", ") + p.nickname;
            const json st = gameStateToJson(s, std::nullopt)["state"];
            // startingScore is only a display hint; meaningful for X01 only.
            const int hint = s.mode == dart::game::GameMode::X01
                                 ? s.options.startingScore : 0;
            db_->saveGame(gm_.gameId(), st.dump(), players, hint);
        }
    });
    gm_.setOnDetected([this](const dart::game::Throw& t,
                             const dart::game::ThrowMeta& m, int dartIndex) {
        const long id = ++throw_seq_;
        ws_.broadcast(dartDetectedToJson(t, m, dartIndex, id).dump());
    });
    det_.setOnBoardStatus([this](const dart::detect::BoardStatus& b) {
        ws_.broadcast(boardStatusToJson(b).dump());
    });
    if (db_)
        gm_.setOnGameOver([this](const dart::game::GameState& s) {
            db_->recordFinishedGame(s);
            db_->deleteSavedGame(gm_.gameId());  // no longer resumable
        });

    // ── Inbound ─────────────────────────────────────────────────────────
    ws_.setOnMessage([this](WsServer::ClientId id, const std::string& text) {
        onMessage_(id, text);
    });
    ws_.setOnConnect([this](WsServer::ClientId id) { onConnect_(id); });
}

void Dispatcher::onConnect_(WsServer::ClientId id) {
    // Bring a freshly connected tablet immediately in sync.
    ws_.sendTo(id, boardStatusToJson(det_.boardStatus()).dump());
    if (gm_.hasGame())
        ws_.sendTo(id, gameStateToJson(gm_.snapshot(), gm_.currentCheckout(), gm_.gameId()).dump());
}

void Dispatcher::onMessage_(WsServer::ClientId id, const std::string& text) {
    json j;
    try {
        j = json::parse(text);
    } catch (const std::exception& e) {
        ws_.sendTo(id, json{{"type", "error"}, {"error", "invalid json"}}.dump());
        return;
    }

    const std::string type = j.value("type", std::string{});
    auto ack = [&](bool ok, const std::string& err = {}) {
        json a{{"type", "ack"}, {"command", type}, {"ok", ok}};
        if (!err.empty()) a["error"] = err;
        ws_.sendTo(id, a.dump());
    };
    auto pushPlayers = [&] {
        json arr = json::array();
        if (db_)
            for (const auto& p : db_->listPlayers())
                arr.push_back({{"id", p.id}, {"nickname", p.nickname},
                               {"totalGames", p.totalGames},
                               {"totalWins", p.totalWins},
                               {"totalLosses", p.totalLosses}});
        ws_.sendTo(id, json{{"type", "players"}, {"players", arr}}.dump());
    };

    try {
        if (type == "create_game") {
            gm_.createGame(playersFromJson(j.value("players", json::array())),
                           configFromJson(j.value("options", json::object())));
            ack(true);
        } else if (type == "manual_throw") {
            gm_.recordManualThrow(throwFromJson(j));
            ack(true);
        } else if (type == "correct_throw") {
            gm_.correctThrow(j.at("turnIndex"), j.at("throwIndex"),
                             throwFromJson(j));
            ack(true);
        } else if (type == "undo") {
            gm_.undo();
            ack(true);
        } else if (type == "next_player") {
            gm_.nextPlayer();
            ack(true);
        } else if (type == "clear_board" || type == "reset_round") {
            det_.resetRound();
            ack(true);
        } else if (type == "refresh_background") {
            det_.refreshBackground();
            ack(true);
        } else if (type == "create_player") {
            int pid = db_ ? db_->upsertPlayer(j.value("nickname", std::string{})) : -1;
            pushPlayers();
            ack(pid >= 0, pid >= 0 ? "" : "no database");
        } else if (type == "rename_player") {
            bool ok = db_ && db_->renamePlayer(j.at("id").get<int>(),
                                               j.value("nickname", std::string{}));
            pushPlayers();
            ack(ok, ok ? "" : "renommage impossible (nom déjà pris ?)");
        } else if (type == "delete_player") {
            bool ok = db_ && db_->deletePlayer(j.at("id").get<int>());
            pushPlayers();
            ack(ok);
        } else if (type == "get_players") {
            pushPlayers();
        } else if (type == "get_leaderboard") {
            json arr = json::array();
            if (db_)
                for (const auto& r : db_->leaderboard(j.value("limit", 10)))
                    arr.push_back({{"id", r.id}, {"nickname", r.nickname},
                                   {"totalGames", r.totalGames},
                                   {"totalWins", r.totalWins},
                                   {"winRate", r.winRate},
                                   {"averageScore", r.averageScore}});
            ws_.sendTo(id, json{{"type", "leaderboard"}, {"leaderboard", arr}}.dump());
        } else if (type == "get_saved_games") {
            json arr = json::array();
            if (db_)
                for (const auto& g : db_->listSavedGames(20))
                    arr.push_back({{"id", g.id}, {"players", g.players},
                                   {"startingScore", g.startingScore},
                                   {"updatedAt", g.updatedAt}});
            ws_.sendTo(id, json{{"type", "saved_games"}, {"games", arr}}.dump());
        } else if (type == "resume_game") {
            const std::string gid = j.value("id", std::string{});
            std::string stateStr = db_ ? db_->loadGameState(gid) : std::string{};
            if (stateStr.empty()) { ack(false, "partie introuvable"); }
            else {
                try {
                    gm_.loadState(gameStateFromJson(json::parse(stateStr)), gid);
                    ack(true);
                } catch (const std::exception& e) { ack(false, e.what()); }
            }
        } else if (type == "get_history") {
            json arr = json::array();
            if (db_)
                for (const auto& g : db_->recentGames(j.value("limit", 20)))
                    arr.push_back({{"id", g.id}, {"mode", g.mode},
                                   {"status", g.status},
                                   {"winnerId", g.winnerId.has_value()
                                        ? json(*g.winnerId) : json(nullptr)},
                                   {"winnerNickname", g.winnerNickname},
                                   {"players", g.players},
                                   {"startingScore", g.startingScore},
                                   {"finishedAt", g.finishedAt}});
            ws_.sendTo(id, json{{"type", "history"}, {"games", arr}}.dump());
        } else if (type == "get_calibration") {
            ws_.sendTo(id, calibrationToJson(det_.calibrationInfo(),
                                             det_.isReplay()).dump());
        } else if (type == "get_camera_snapshot") {
            const int maxW = j.value("maxWidth", 480);
            json arr = json::array();
            auto addCam = [&](int c) {
                std::string b64 = det_.cameraSnapshotJpeg(c, maxW);
                if (!b64.empty()) arr.push_back({{"camId", c}, {"jpeg", b64}});
            };
            if (j.contains("cam")) addCam(j.at("cam").get<int>());
            else for (int c = 0; c < dart::detect::NUM_CAMS; ++c) addCam(c);
            ws_.sendTo(id, json{{"type", "camera_snapshot"},
                                {"cams", arr}}.dump());
        } else if (type == "run_autocalib") {
            const int cam = j.at("cam").get<int>();
            const auto opt =
                autoCalibOptionsFromJson(j.value("options", json::object()));
            const auto res = det_.runAutoCalib(cam, opt);
            ws_.sendTo(id, autoCalibResultToJson(cam, res).dump());
            ack(res.ok, res.ok ? "" : res.error);
        } else if (type == "save_calibration") {
            const int cam = j.at("cam").get<int>();
            std::string err;
            const bool ok = det_.saveCalibration(cam, err);
            ack(ok, err);
            ws_.sendTo(id, calibrationToJson(det_.calibrationInfo(),
                                             det_.isReplay()).dump());
        } else {
            ack(false, "unknown command: " + type);
        }
    } catch (const std::exception& e) {
        ack(false, e.what());
    }
}

} // namespace dart::api
