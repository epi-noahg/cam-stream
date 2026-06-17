#pragma once

// Dispatcher — glues the WebSocket server to the authoritative game + detection.
//
// Outbound: subscribes to GameManager / DetectionService callbacks and
// broadcasts game_state / dart_detected / board_status to all clients.
// Inbound: parses client commands and drives the corresponding mutation, then
// replies with an ack.  Fresh clients are sent the current state on connect.

#include "WsServer.hpp"

#include <atomic>
#include <cstdint>

namespace dart::game    { class GameManager; }
namespace dart::detect  { class DetectionService; }
namespace dart::persist { class Db; }

namespace dart::api {

class Dispatcher {
public:
    /// @param db optional persistence layer; when null, read commands return
    ///           empty results and finished games are not recorded.
    Dispatcher(WsServer& ws, dart::game::GameManager& gm,
               dart::detect::DetectionService& det,
               dart::persist::Db* db = nullptr);

    /// Install callbacks on the game/detection/ws objects.  Call once before
    /// starting the server.
    void wire();

private:
    void onMessage_(WsServer::ClientId id, const std::string& text);
    void onConnect_(WsServer::ClientId id);

    WsServer&                        ws_;
    dart::game::GameManager&         gm_;
    dart::detect::DetectionService&  det_;
    dart::persist::Db*               db_ {nullptr};
    std::atomic<long>                throw_seq_{0};
};

} // namespace dart::api
