#include "Display.hpp"
#include "StreamReceiver.hpp"
#include "VideoDecoder.hpp"
#include "Protocol.hpp"

#include <arpa/inet.h>

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
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
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server-ip> [port]\n"
                  << "  server-ip  IP address or hostname of the Ubuntu server\n"
                  << "  port       TCP port (default: " << DEFAULT_PORT << ")\n";
        return 1;
    }

    const std::string host = argv[1];
    const uint16_t    port = (argc >= 3)
                             ? static_cast<uint16_t>(std::stoi(argv[2]))
                             : DEFAULT_PORT;

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    // ── Shared objects ────────────────────────────────────────────────────────
    // Display dimensions are set from the first Init packet; we use placeholders
    // here and the server always sends 640×480 by default.
    constexpr int WIDTH  = 640;
    constexpr int HEIGHT = 480;

    Display display(MAX_CAMERAS, WIDTH, HEIGHT);

    std::vector<std::unique_ptr<VideoDecoder>> decoders;
    decoders.reserve(MAX_CAMERAS);
    for (int i = 0; i < MAX_CAMERAS; ++i)
        decoders.push_back(std::make_unique<VideoDecoder>());

    // ── Packet handler (runs on the receive thread) ───────────────────────────
    auto onPacket = [&](ReceivedPacket pkt) {
        if (pkt.cam_id < 0 || pkt.cam_id >= MAX_CAMERAS) return;

        if (pkt.type == PacketType::Init) {
            // Parse InitPayloadHeader + extradata
            if (pkt.data.size() < sizeof(InitPayloadHeader)) return;

            const auto* iph = reinterpret_cast<const InitPayloadHeader*>(
                pkt.data.data());
            const int w   = static_cast<int>(ntohl(iph->width));
            const int h   = static_cast<int>(ntohl(iph->height));
            const int extra_sz = static_cast<int>(
                pkt.data.size() - sizeof(InitPayloadHeader));
            const uint8_t* extra = pkt.data.data() + sizeof(InitPayloadHeader);

            if (decoders[pkt.cam_id]->init(w, h, extra, extra_sz)) {
                std::cout << "[main] Decoder ready: cam" << pkt.cam_id
                          << " " << w << "x" << h << "\n";
            }
        }
        else if (pkt.type == PacketType::Video) {
            auto& dec = decoders[pkt.cam_id];
            if (!dec->isInitialized()) return;

            cv::Mat frame = dec->decode(pkt.data.data(),
                                        static_cast<int>(pkt.data.size()));
            if (!frame.empty())
                display.updateFrame(pkt.cam_id, std::move(frame));
        }
    };

    // ── Connect and start receiving ───────────────────────────────────────────
    StreamReceiver receiver(host, port);
    receiver.start(std::move(onPacket));

    std::cout << "[main] Connecting to " << host << ":" << port << "\n"
              << "[main] Press ESC or close the window to quit\n";

    // ── Main display loop (must run on the main thread on macOS) ─────────────
    while (g_running && receiver.isConnected()) {
        if (!display.render()) break;
    }

    // If the receiver never connected, wait briefly and exit cleanly
    if (!receiver.isConnected() && g_running) {
        std::cerr << "[main] Could not reach server – check IP and port\n";
    }

    std::cout << "[main] Shutting down...\n";
    receiver.stop();
    return 0;
}
