#include "StreamServer.hpp"
#include "VideoEncoder.hpp"
#include "Protocol.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace camstream {

// ── Helper: serialise a packet into a flat byte buffer ───────────────────────

static std::vector<uint8_t> buildVideoPacket(const EncodedPacket& pkt)
{
    PacketHeader hdr{};
    hdr.magic     = htonl(PACKET_MAGIC);
    hdr.type      = static_cast<uint8_t>(PacketType::Video);
    hdr.cam_id    = static_cast<uint8_t>(pkt.cam_id);
    hdr.flags     = pkt.is_keyframe ? 0x01u : 0x00u;
    hdr.reserved  = 0;
    hdr.pts_ms    = htonl(static_cast<uint32_t>(pkt.pts_ms));
    hdr.data_size = htonl(static_cast<uint32_t>(pkt.data.size()));

    std::vector<uint8_t> buf(sizeof(PacketHeader) + pkt.data.size());
    std::memcpy(buf.data(), &hdr, sizeof(PacketHeader));
    std::memcpy(buf.data() + sizeof(PacketHeader), pkt.data.data(), pkt.data.size());
    return buf;
}

// ── StreamServer ─────────────────────────────────────────────────────────────

StreamServer::StreamServer(uint16_t port)
    : port_(port)
{
    init_packets_.resize(MAX_CAMERAS);
}

StreamServer::~StreamServer()
{
    stop();
}

void StreamServer::start()
{
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) throw std::runtime_error("socket() failed");

    const int opt = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed");

    if (::listen(listen_fd_, 8) < 0)
        throw std::runtime_error("listen() failed");

    running_       = true;
    accept_thread_ = std::thread(&StreamServer::acceptLoop, this);
    send_thread_   = std::thread(&StreamServer::sendLoop,   this);

    std::cout << "[StreamServer] Listening on port " << port_ << "\n";
}

void StreamServer::stop()
{
    if (!running_.exchange(false)) return;  // already stopped

    // Unblock accept() by closing the listen socket
    if (listen_fd_ >= 0) { ::close(listen_fd_); listen_fd_ = -1; }

    // Unblock the send thread
    queue_cv_.notify_all();

    if (accept_thread_.joinable()) accept_thread_.join();
    if (send_thread_.joinable())   send_thread_.join();

    std::lock_guard<std::mutex> lk(clients_mutex_);
    for (int fd : clients_) ::close(fd);
    clients_.clear();
}

void StreamServer::setInitPacket(int cam_id, std::vector<uint8_t> wire_packet)
{
    std::lock_guard<std::mutex> lk(init_mutex_);
    init_packets_[cam_id] = std::move(wire_packet);
}

void StreamServer::push(const EncodedPacket& pkt)
{
    auto buf = buildVideoPacket(pkt);
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        send_queue_.push(std::move(buf));
    }
    queue_cv_.notify_one();
}

// ── Private threads ───────────────────────────────────────────────────────────

void StreamServer::acceptLoop()
{
    while (running_) {
        sockaddr_in  client_addr{};
        socklen_t    client_len = sizeof(client_addr);
        const int fd = ::accept(listen_fd_,
                                reinterpret_cast<sockaddr*>(&client_addr),
                                &client_len);
        if (fd < 0) {
            if (running_) std::cerr << "[StreamServer] accept() error\n";
            break;
        }

        std::cout << "[StreamServer] Client connected: "
                  << ::inet_ntoa(client_addr.sin_addr) << "\n";

        // Send all stored Init packets so the client can initialise its decoders
        {
            std::lock_guard<std::mutex> lk(init_mutex_);
            for (const auto& init_pkt : init_packets_) {
                if (!init_pkt.empty())
                    sendAll(fd, init_pkt.data(), init_pkt.size());
            }
        }

        std::lock_guard<std::mutex> lk(clients_mutex_);
        clients_.push_back(fd);
    }
}

void StreamServer::sendLoop()
{
    while (running_) {
        std::vector<uint8_t> buf;
        {
            std::unique_lock<std::mutex> lk(queue_mutex_);
            queue_cv_.wait(lk, [this] {
                return !send_queue_.empty() || !running_;
            });
            if (!running_ && send_queue_.empty()) break;
            buf = std::move(send_queue_.front());
            send_queue_.pop();
        }

        std::lock_guard<std::mutex> lk(clients_mutex_);
        std::vector<int> dead;

        for (const int fd : clients_) {
            if (!sendAll(fd, buf.data(), buf.size()))
                dead.push_back(fd);
        }

        for (const int fd : dead) {
            ::close(fd);
            clients_.erase(std::remove(clients_.begin(), clients_.end(), fd),
                           clients_.end());
            std::cout << "[StreamServer] Client disconnected\n";
        }
    }
}

bool StreamServer::sendAll(int fd, const uint8_t* data, size_t size)
{
    size_t sent = 0;
    while (sent < size) {
        const ssize_t n = ::send(fd, data + sent, size - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

} // namespace camstream
