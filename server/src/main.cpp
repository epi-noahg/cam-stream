#include "CameraCapture.hpp"
#include "VideoEncoder.hpp"
#include "StreamServer.hpp"
#include "Protocol.hpp"

#include <arpa/inet.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace camstream;

// ── Graceful shutdown ─────────────────────────────────────────────────────────

namespace {
    std::atomic<bool> g_running{true};
    void onSignal(int) { g_running = false; }
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    // ── Configuration ─────────────────────────────────────────────────────────
    constexpr int      WIDTH        = 640;
    constexpr int      HEIGHT       = 480;
    constexpr int      FPS          = 30;
    constexpr int      BITRATE_KBPS = 500;
    constexpr uint16_t PORT         = DEFAULT_PORT;

    // Camera device paths; override positional args if supplied
    std::vector<std::string> devices = {
        "/dev/video0", "/dev/video1", "/dev/video2"
    };
    for (int i = 1; i < argc && i - 1 < MAX_CAMERAS; ++i)
        devices[i - 1] = argv[i];

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    // ── Create server ─────────────────────────────────────────────────────────
    StreamServer server(PORT);

    // ── Create encoders and register Init packets ─────────────────────────────
    std::vector<std::unique_ptr<VideoEncoder>> encoders;
    encoders.reserve(MAX_CAMERAS);

    for (int i = 0; i < MAX_CAMERAS; ++i) {
        auto enc = std::make_unique<VideoEncoder>(i, WIDTH, HEIGHT, FPS, BITRATE_KBPS);

        // Build Init payload: InitPayloadHeader + raw SPS/PPS extradata
        InitPayloadHeader iph{};
        iph.width  = htonl(static_cast<uint32_t>(WIDTH));
        iph.height = htonl(static_cast<uint32_t>(HEIGHT));
        iph.fps    = htonl(static_cast<uint32_t>(FPS));

        const auto& extra = enc->extradata();
        const size_t payload_size = sizeof(iph) + extra.size();

        // Wrap in a complete wire packet (header + payload)
        PacketHeader hdr{};
        hdr.magic     = htonl(PACKET_MAGIC);
        hdr.type      = static_cast<uint8_t>(PacketType::Init);
        hdr.cam_id    = static_cast<uint8_t>(i);
        hdr.flags     = 0;
        hdr.reserved  = 0;
        hdr.pts_ms    = 0;
        hdr.data_size = htonl(static_cast<uint32_t>(payload_size));

        std::vector<uint8_t> wire(sizeof(PacketHeader) + payload_size);
        std::memcpy(wire.data(), &hdr, sizeof(PacketHeader));
        std::memcpy(wire.data() + sizeof(PacketHeader), &iph, sizeof(iph));
        if (!extra.empty())
            std::memcpy(wire.data() + sizeof(PacketHeader) + sizeof(iph),
                        extra.data(), extra.size());

        server.setInitPacket(i, std::move(wire));
        encoders.push_back(std::move(enc));
    }

    server.start();

    // ── Start capture threads ─────────────────────────────────────────────────
    std::vector<std::unique_ptr<CameraCapture>> captures;
    captures.reserve(MAX_CAMERAS);

    for (int i = 0; i < MAX_CAMERAS; ++i) {
        auto cap = std::make_unique<CameraCapture>(i, devices[i],
                                                   WIDTH, HEIGHT, FPS);
        VideoEncoder* enc = encoders[i].get();

        cap->start([enc, &server](RawFrame rf) {
            // This lambda runs in the capture thread
            auto pkts = enc->encode(rf.bgr, rf.timestamp_ms);
            for (auto& pkt : pkts)
                server.push(pkt);
        });

        captures.push_back(std::move(cap));
    }

    std::cout << "[main] Streaming " << MAX_CAMERAS
              << " cameras on port " << PORT << "\n"
              << "[main] Press Ctrl+C to stop\n";

    while (g_running)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // ── Graceful shutdown ─────────────────────────────────────────────────────
    std::cout << "[main] Shutting down...\n";
    for (auto& cap : captures) cap->stop();
    for (auto& enc : encoders) enc->flush();
    server.stop();

    return 0;
}
