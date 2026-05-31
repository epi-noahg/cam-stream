#include "StreamReceiver.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

namespace camstream {

StreamReceiver::StreamReceiver(const std::string& host, uint16_t port)
    : host_(host), port_(port)
{}

StreamReceiver::~StreamReceiver()
{
    stop();
}

void StreamReceiver::start(PacketCallback callback)
{
    callback_ = std::move(callback);
    running_  = true;
    thread_   = std::thread(&StreamReceiver::receiveLoop, this);
}

void StreamReceiver::stop()
{
    running_ = false;
    if (sock_fd_ >= 0) { ::close(sock_fd_); sock_fd_ = -1; }
    if (thread_.joinable()) thread_.join();
}

void StreamReceiver::receiveLoop()
{
    // Resolve the server address
    addrinfo hints{};
    addrinfo* res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    const std::string port_str = std::to_string(port_);
    if (::getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &res) != 0) {
        std::cerr << "[StreamReceiver] Cannot resolve host: " << host_ << "\n";
        running_ = false;
        return;
    }

    sock_fd_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_fd_ < 0) {
        std::cerr << "[StreamReceiver] socket() failed\n";
        ::freeaddrinfo(res);
        running_ = false;
        return;
    }

    if (::connect(sock_fd_, res->ai_addr, res->ai_addrlen) != 0) {
        std::cerr << "[StreamReceiver] Cannot connect to "
                  << host_ << ":" << port_ << "\n";
        ::freeaddrinfo(res);
        ::close(sock_fd_); sock_fd_ = -1;
        running_ = false;
        return;
    }
    ::freeaddrinfo(res);
    connected_ = true;
    std::cout << "[StreamReceiver] Connected to " << host_ << ":" << port_ << "\n";

    // Main receive loop: read header → read payload → dispatch
    while (running_) {
        PacketHeader hdr{};
        if (!recvAll(reinterpret_cast<uint8_t*>(&hdr), sizeof(hdr))) break;

        if (ntohl(hdr.magic) != PACKET_MAGIC) {
            std::cerr << "[StreamReceiver] Bad magic – stream desync\n";
            break;
        }

        const uint32_t data_size = ntohl(hdr.data_size);
        std::vector<uint8_t> data(data_size);
        if (data_size > 0 && !recvAll(data.data(), data_size)) break;

        ReceivedPacket pkt{};
        pkt.type        = static_cast<PacketType>(hdr.type);
        pkt.cam_id      = static_cast<int>(hdr.cam_id);
        pkt.is_keyframe = (hdr.flags & 0x01u) != 0;
        pkt.pts_ms      = ntohl(hdr.pts_ms);
        pkt.data        = std::move(data);

        callback_(pkt);
    }

    connected_ = false;
    std::cout << "[StreamReceiver] Disconnected\n";
}

bool StreamReceiver::recvAll(uint8_t* buf, size_t size)
{
    size_t received = 0;
    while (received < size) {
        const ssize_t n = ::recv(sock_fd_, buf + received, size - received, 0);
        if (n <= 0) return false;
        received += static_cast<size_t>(n);
    }
    return true;
}

} // namespace camstream
