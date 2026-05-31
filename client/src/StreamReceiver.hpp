#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "Protocol.hpp"

namespace camstream {

/// One packet received from the server (fully parsed, payload already copied).
struct ReceivedPacket {
    PacketType           type;
    int                  cam_id;
    bool                 is_keyframe;
    uint32_t             pts_ms;
    std::vector<uint8_t> data;
};

using PacketCallback = std::function<void(ReceivedPacket)>;

/// Opens a TCP connection to a StreamServer and dispatches every received
/// packet to a user-supplied callback (called from the receive thread).
class StreamReceiver {
public:
    /// @param host  Server IP address or hostname.
    /// @param port  Server TCP port.
    StreamReceiver(const std::string& host, uint16_t port);
    ~StreamReceiver();

    // Non-copyable, non-movable
    StreamReceiver(const StreamReceiver&)            = delete;
    StreamReceiver& operator=(const StreamReceiver&) = delete;

    /// Connect and start the receive thread.
    void start(PacketCallback callback);

    /// Stop the receive thread and close the socket.
    void stop();

    bool isConnected() const noexcept { return connected_.load(); }

private:
    void receiveLoop();

    /// Read exactly @p size bytes from the socket; returns false on error/EOF.
    bool recvAll(uint8_t* buf, size_t size);

    std::string host_;
    uint16_t    port_;
    int         sock_fd_{-1};

    PacketCallback    callback_;
    std::thread       thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
};

} // namespace camstream
