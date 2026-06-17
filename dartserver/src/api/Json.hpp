#pragma once

// JSON (de)serialization for the WebSocket protocol.  All shapes mirror the
// front-end types in GoDartss so the UI maps them 1:1.  This is the only place
// that knows the wire format; game/detection layers stay JSON-agnostic.

#include "detection/DetectionTypes.hpp"
#include "game/GameManager.hpp"
#include "game/GameTypes.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <vector>

namespace dart::api {

using nlohmann::json;

// ── enum <-> string (front-end vocabulary) ──────────────────────────────────
const char*       inOutToString(game::InOutType t);
game::InOutType   inOutFromString(const std::string& s);
const char*       scoringToString(game::ScoringMode m);
game::ScoringMode scoringFromString(const std::string& s);

// ── serialize ───────────────────────────────────────────────────────────────
json throwToJson(const game::Throw& t);
json optionsToJson(const game::OptionsX01& o);
json gameStateToJson(const game::GameState& s,
                     const std::optional<std::vector<game::Throw>>& checkout);
json boardStatusToJson(const detect::BoardStatus& b);

/// Build the `dart_detected` event payload.
json dartDetectedToJson(const game::Throw& t, const game::ThrowMeta& m,
                        int dartIndex, long throwId);

/// Parse a serialized GameState (the `state` object of a game_state message)
/// back into a GameState — used to resume a saved game.
game::GameState gameStateFromJson(const json& j);

// ── parse (client → server) ─────────────────────────────────────────────────
game::OptionsX01 optionsFromJson(const json& j);
/// Parse a list of {id, nickname} players.  Missing ids are auto-assigned.
std::vector<game::PlayerState> playersFromJson(const json& j);
game::Throw      throwFromJson(const json& j);

} // namespace dart::api
