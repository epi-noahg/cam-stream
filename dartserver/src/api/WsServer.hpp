#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// WsServer — a small WebSocket server (Boost.Beast, synchronous, one thread per
// connection).  Built for a handful of clients (one or two tablets), so the
// simple thread-per-connection model is plenty and avoids async ceremony.
//
//   • inbound text frames are handed to the on-message callback (with the
//     originating client id) on the connection's own thread;
//   • broadcast()/sendTo() push text to clients; each session serializes its
//     own writes with a mutex so broadcasts and acks can't interleave.
// ─────────────────────────────────────────────────────────────────────────────

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace dart::api {

class WsSession;  // defined in the .cpp

class WsServer {
public:
    using ClientId        = std::uint64_t;
    using MessageHandler  = std::function<void(ClientId, const std::string&)>;
    using ConnectHandler  = std::function<void(ClientId)>;

    explicit WsServer(std::uint16_t port);
    ~WsServer();

    void setOnMessage(MessageHandler h) { on_message_ = std::move(h); }
    /// Called when a client finishes the handshake — use it to push the
    /// current state so a fresh tablet is immediately in sync.
    void setOnConnect(ConnectHandler h) { on_connect_ = std::move(h); }

    void start();
    void stop();

    void broadcast(const std::string& msg);
    void sendTo(ClientId id, const std::string& msg);

private:
    friend class WsSession;
    void acceptLoop_();
    void registerSession_(ClientId id, std::shared_ptr<WsSession> s);
    void removeSession_(ClientId id);
    void dispatchMessage_(ClientId id, const std::string& msg);
    void dispatchConnect_(ClientId id);

    std::uint16_t                                       port_;
    std::atomic<bool>                                   running_{false};
    std::thread                                         accept_thread_;
    MessageHandler                                      on_message_;
    ConnectHandler                                      on_connect_;

    std::mutex                                          sessions_mtx_;
    std::map<ClientId, std::shared_ptr<WsSession>>      sessions_;
    std::atomic<ClientId>                               next_id_{1};

    struct Impl;                       // hides the asio acceptor
    std::unique_ptr<Impl>                               impl_;
};

} // namespace dart::api
