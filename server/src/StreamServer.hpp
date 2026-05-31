#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace camstream {

struct EncodedPacket;

/// Listens for TCP clients and broadcasts encoded packets to all of them.
///
/// Thread-safety:
///   - push()          may be called from any thread concurrently.
///   - setInitPacket() must be called before start().
///   - stop()          is safe to call more than once.
class StreamServer {
public:
    explicit StreamServer(uint16_t port);
    ~StreamServer();

    // Non-copyable, non-movable
    StreamServer(const StreamServer&)            = delete;
    StreamServer& operator=(const StreamServer&) = delete;

    /// Bind the socket and spawn accept/send threads.
    void start();

    /// Gracefully stop: close all connections and join threads.
    void stop();

    /// Enqueue one encoded packet for broadcast.  Non-blocking.
    void push(const EncodedPacket& pkt);

    /// Store the complete wire-format Init packet for camera @p cam_id.
    /// New clients receive all stored Init packets before any Video packets.
    void setInitPacket(int cam_id, std::vector<uint8_t> wire_packet);

private:
    /// Accepts incoming TCP connections and sends Init packets to each new client.
    void acceptLoop();

    /// Reads from the send queue and writes to every connected client.
    void sendLoop();

    /// Write exactly @p size bytes to @p fd.  Returns false on error.
    static bool sendAll(int fd, const uint8_t* data, size_t size);

    uint16_t port_;
    int      listen_fd_{-1};
    std::atomic<bool> running_{false};

    std::thread accept_thread_;
    std::thread send_thread_;

    // Connected client file descriptors
    std::mutex       clients_mutex_;
    std::vector<int> clients_;

    // Thread-safe queue of serialised packets waiting to be sent
    std::mutex                               queue_mutex_;
    std::condition_variable                  queue_cv_;
    std::queue<std::vector<uint8_t>>         send_queue_;

    // Per-camera Init packets (complete wire format, sent to new clients)
    std::mutex                               init_mutex_;
    std::vector<std::vector<uint8_t>>        init_packets_;
};

} // namespace camstream
