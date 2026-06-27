#include "Json.hpp"

namespace dart::api {

const char* inOutToString(game::InOutType t) {
    switch (t) {
        case game::InOutType::Any:    return "ANY";
        case game::InOutType::Double: return "DOUBLE";
        case game::InOutType::Triple: return "TRIPLE";
    }
    return "ANY";
}

game::InOutType inOutFromString(const std::string& s) {
    if (s == "DOUBLE") return game::InOutType::Double;
    if (s == "TRIPLE") return game::InOutType::Triple;
    return game::InOutType::Any;
}

const char* scoringToString(game::ScoringMode m) {
    return m == game::ScoringMode::BestOf ? "BEST_OF" : "FIRST_TO";
}

game::ScoringMode scoringFromString(const std::string& s) {
    return s == "BEST_OF" ? game::ScoringMode::BestOf : game::ScoringMode::FirstTo;
}

const char* gameModeToString(game::GameMode m) {
    switch (m) {
        case game::GameMode::X01:           return "X01";
        case game::GameMode::Cricket:       return "CRICKET";
        case game::GameMode::RoundTheClock: return "ROUND_THE_CLOCK";
    }
    return "X01";
}

game::GameMode gameModeFromString(const std::string& s) {
    if (s == "CRICKET")         return game::GameMode::Cricket;
    if (s == "ROUND_THE_CLOCK") return game::GameMode::RoundTheClock;
    return game::GameMode::X01;
}

json throwToJson(const game::Throw& t) {
    return json{{"value", t.value}, {"multiplier", t.multiplier},
                {"bust", t.bust}};
}

json optionsToJson(const game::OptionsX01& o) {
    return json{
        {"mode",          "X01"},
        {"startingScore", o.startingScore},
        {"inType",        inOutToString(o.inType)},
        {"outType",       inOutToString(o.outType)},
        {"scoringMode",   scoringToString(o.scoringMode)},
        {"legs",          o.legs},
        {"allowBust",     o.allowBust},
        {"teams",         o.teams},
    };
}

json optionsToJson(const game::GameState& s) {
    switch (s.mode) {
        case game::GameMode::Cricket:
            return json{
                {"mode",        "CRICKET"},
                {"cutThroat",   s.cricket.cutThroat},
                {"useBull",     s.cricket.useBull},
                {"scoringMode", scoringToString(s.cricket.scoringMode)},
                {"legs",        s.cricket.legs},
                {"teams",       s.cricket.teams},
            };
        case game::GameMode::RoundTheClock:
            return json{
                {"mode",        "ROUND_THE_CLOCK"},
                {"scoringMode", scoringToString(s.roundClock.scoringMode)},
                {"legs",        s.roundClock.legs},
                {"teams",       s.roundClock.teams},
            };
        case game::GameMode::X01:
            break;
    }
    return optionsToJson(s.options);
}

json gameStateToJson(const game::GameState& s,
                     const std::optional<std::vector<game::Throw>>& checkout,
                     const std::string& id) {
    json players = json::array();
    for (const auto& p : s.players) {
        json throws = json::array();
        for (const auto& t : p.throws) throws.push_back(throwToJson(t));
        json pj{
            {"id", p.id},
            {"nickname", p.nickname},
            {"score", p.score},
            {"legsWon", p.legsWon},
            {"team", p.team},
            {"throws", throws},
        };
        // Mode-specific state, emitted only where it applies.
        if (s.mode == game::GameMode::Cricket) pj["marks"]  = p.marks;
        if (s.mode == game::GameMode::RoundTheClock) pj["target"] = p.target;
        players.push_back(std::move(pj));
    }

    json turns = json::array();
    for (const auto& turn : s.turns) {
        json jt = json::array();
        for (const auto& t : turn) jt.push_back(throwToJson(t));
        turns.push_back(jt);
    }

    json checkoutJson = nullptr;
    if (checkout.has_value()) {
        checkoutJson = json::array();
        for (const auto& t : *checkout) checkoutJson.push_back(throwToJson(t));
    }

    json out{
        {"type", "game_state"},
        {"state", json{
            {"mode", gameModeToString(s.mode)},
            {"options", optionsToJson(s)},
            {"players", players},
            {"currentIndex", s.currentIndex},
            {"dartIndex", s.dartIndex},
            {"turns", turns},
            {"winner", s.winner.has_value() ? json(*s.winner) : json(nullptr)},
            {"finishedPlayers", s.finishedPlayers},
            {"gameOver", s.gameOver},
        }},
        {"checkout", checkoutJson},
    };
    // Stable match id so clients can tell one game from the next (e.g. to keep
    // the new-game setup screen until a freshly created game actually arrives).
    if (!id.empty()) out["id"] = id;
    return out;
}

json boardStatusToJson(const detect::BoardStatus& b) {
    json cams = json::array();
    for (const auto& c : b.cams)
        cams.push_back(json{{"id", c.id},
                            {"state", detect::toString(c.state)},
                            {"ready", c.ready}});
    return json{
        {"type", "board_status"},
        {"cams", cams},
        {"round", json{{"phase", b.round.phase},
                       {"nextDart", b.round.nextDart},
                       {"message", b.round.message}}},
        {"allReady", b.allReady},
    };
}

json calibrationToJson(const std::vector<detect::CalibCamInfo>& cams, bool replay) {
    json arr = json::array();
    for (const auto& c : cams)
        arr.push_back(json{
            {"camId",          c.camId},
            {"calibPath",      c.calibPath},
            {"zonesPath",      c.zonesPath},
            {"hasCalib",       c.hasCalib},
            {"hasZones",       c.hasZones},
            {"width",          c.width},
            {"height",         c.height},
            {"orientationDeg", c.orientationDeg},
            {"diffThreshold",  c.diffThreshold},
        });
    return json{{"type", "calibration"}, {"replay", replay}, {"cams", arr}};
}

json autoCalibResultToJson(int cam, const detect::AutoCalibOutcome& r) {
    return json{
        {"type",            "autocalib_result"},
        {"camId",           cam},
        {"ok",              r.ok},
        {"error",           r.error},
        {"warning",         r.warning},
        {"triplesFound",    r.triplesFound},
        {"doublesFound",    r.doublesFound},
        {"meanReprojErrPx", r.meanReprojErrPx},
        {"redADelta",       r.redADelta},
        {"greenADelta",     r.greenADelta},
        {"minChroma",       r.minChroma},
        {"overlay",         r.overlayBase64},
    };
}

json dartDetectedToJson(const game::Throw& t, const game::ThrowMeta& m,
                        int dartIndex, long throwId) {
    return json{
        {"type", "dart_detected"},
        {"throwId", throwId},
        {"value", t.value},
        {"multiplier", t.multiplier},
        {"score", t.hitValue()},
        {"zone", m.zone},
        {"confidence", m.confidence},
        {"dartIndex", dartIndex},
        {"needsReview", m.needsReview},
    };
}

game::GameState gameStateFromJson(const json& j) {
    game::GameState s;
    const json optsJson = j.value("options", json::object());
    const game::GameConfig cfg = configFromJson(optsJson);
    s.mode       = cfg.mode;
    s.options    = cfg.x01;
    s.cricket    = cfg.cricket;
    s.roundClock = cfg.roundClock;
    s.players.clear();
    for (const auto& pj : j.value("players", json::array())) {
        game::PlayerState p;
        p.id       = pj.value("id", 0);
        p.nickname = pj.value("nickname", std::string{});
        p.score    = pj.value("score", s.mode == game::GameMode::X01
                                           ? s.options.startingScore : 0);
        p.legsWon  = pj.value("legsWon", 0);
        p.team     = pj.value("team", 0);
        for (const auto& tj : pj.value("throws", json::array()))
            p.throws.push_back(throwFromJson(tj));
        if (pj.contains("marks") && pj["marks"].is_array())
            p.marks = pj["marks"].get<std::vector<int>>();
        p.target = pj.value("target", 0);
        s.players.push_back(std::move(p));
    }
    s.currentIndex = j.value("currentIndex", 0);
    s.dartIndex    = j.value("dartIndex", 0);
    s.turns.clear();
    for (const auto& turn : j.value("turns", json::array())) {
        std::vector<game::Throw> t;
        for (const auto& tj : turn) t.push_back(throwFromJson(tj));
        s.turns.push_back(std::move(t));
    }
    if (s.turns.empty()) s.turns.push_back({});
    if (j.contains("winner") && !j["winner"].is_null())
        s.winner = j["winner"].get<int>();
    s.finishedPlayers = j.value("finishedPlayers", std::vector<int>{});
    s.gameOver = j.value("gameOver", false);
    return s;
}

game::OptionsX01 optionsFromJson(const json& j) {
    game::OptionsX01 o;
    if (j.is_null()) return o;
    o.startingScore = j.value("startingScore", 501);
    o.inType        = inOutFromString(j.value("inType", std::string("ANY")));
    o.outType       = inOutFromString(j.value("outType", std::string("DOUBLE")));
    o.scoringMode   = scoringFromString(j.value("scoringMode", std::string("FIRST_TO")));
    o.legs          = j.value("legs", 1);
    o.allowBust     = j.value("allowBust", false);
    o.teams         = j.value("teams", 1);
    return o;
}

game::GameConfig configFromJson(const json& j) {
    game::GameConfig cfg;
    if (j.is_null()) return cfg;
    cfg.mode = gameModeFromString(j.value("mode", std::string("X01")));
    switch (cfg.mode) {
        case game::GameMode::Cricket:
            cfg.cricket.cutThroat   = j.value("cutThroat", false);
            cfg.cricket.useBull     = j.value("useBull", true);
            cfg.cricket.scoringMode = scoringFromString(
                j.value("scoringMode", std::string("FIRST_TO")));
            cfg.cricket.legs        = j.value("legs", 1);
            cfg.cricket.teams       = j.value("teams", 1);
            break;
        case game::GameMode::RoundTheClock:
            cfg.roundClock.scoringMode = scoringFromString(
                j.value("scoringMode", std::string("FIRST_TO")));
            cfg.roundClock.legs        = j.value("legs", 1);
            cfg.roundClock.teams       = j.value("teams", 1);
            break;
        case game::GameMode::X01:
            cfg.x01 = optionsFromJson(j);
            break;
    }
    return cfg;
}

std::vector<game::PlayerState> playersFromJson(const json& j) {
    std::vector<game::PlayerState> players;
    if (!j.is_array()) return players;
    int autoId = 0;
    for (const auto& p : j) {
        game::PlayerState ps;
        ps.id       = p.value("id", autoId);
        ps.nickname = p.value("nickname", std::string("P") + std::to_string(ps.id));
        ps.team     = p.value("team", 0);
        players.push_back(std::move(ps));
        ++autoId;
    }
    return players;
}

game::Throw throwFromJson(const json& j) {
    game::Throw t;
    t.value      = j.value("value", 0);
    t.multiplier = j.value("multiplier", 1);
    t.bust       = j.value("bust", false);
    return t;
}

detect::AutoCalibOptions autoCalibOptionsFromJson(const json& j) {
    detect::AutoCalibOptions o;
    if (!j.is_object()) return o;
    o.redADelta      = j.value("redADelta", o.redADelta);
    o.greenADelta    = j.value("greenADelta", o.greenADelta);
    o.minChroma      = j.value("minChroma", o.minChroma);
    o.sector20Offset = j.value("sector20Offset", o.sector20Offset);
    o.sector20HintX  = j.value("sector20HintX", o.sector20HintX);
    o.sector20HintY  = j.value("sector20HintY", o.sector20HintY);
    o.autotune       = j.value("autotune", o.autotune);
    return o;
}

} // namespace dart::api
