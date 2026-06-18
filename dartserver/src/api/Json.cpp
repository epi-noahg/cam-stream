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

json throwToJson(const game::Throw& t) {
    return json{{"value", t.value}, {"multiplier", t.multiplier},
                {"bust", t.bust}};
}

json optionsToJson(const game::OptionsX01& o) {
    return json{
        {"startingScore", o.startingScore},
        {"inType",        inOutToString(o.inType)},
        {"outType",       inOutToString(o.outType)},
        {"scoringMode",   scoringToString(o.scoringMode)},
        {"legs",          o.legs},
        {"allowBust",     o.allowBust},
        {"teams",         o.teams},
    };
}

json gameStateToJson(const game::GameState& s,
                     const std::optional<std::vector<game::Throw>>& checkout) {
    json players = json::array();
    for (const auto& p : s.players) {
        json throws = json::array();
        for (const auto& t : p.throws) throws.push_back(throwToJson(t));
        players.push_back(json{
            {"id", p.id},
            {"nickname", p.nickname},
            {"score", p.score},
            {"legsWon", p.legsWon},
            {"team", p.team},
            {"throws", throws},
        });
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

    return json{
        {"type", "game_state"},
        {"state", json{
            {"options", optionsToJson(s.options)},
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
    s.options = optionsFromJson(j.value("options", json::object()));
    s.players.clear();
    for (const auto& pj : j.value("players", json::array())) {
        game::PlayerState p;
        p.id       = pj.value("id", 0);
        p.nickname = pj.value("nickname", std::string{});
        p.score    = pj.value("score", s.options.startingScore);
        p.legsWon  = pj.value("legsWon", 0);
        p.team     = pj.value("team", 0);
        for (const auto& tj : pj.value("throws", json::array()))
            p.throws.push_back(throwFromJson(tj));
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

} // namespace dart::api
