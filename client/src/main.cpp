#include "Display.hpp"
#include "Recorder.hpp"
#include "StreamReceiver.hpp"
#include "VideoDecoder.hpp"
#include "Protocol.hpp"

#include <arpa/inet.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace camstream;

// ── Graceful shutdown ─────────────────────────────────────────────────────────

namespace {
    std::atomic<bool> g_running{true};
    void onSignal(int) { g_running = false; }
}

// ── Argument parsing ──────────────────────────────────────────────────────────

struct Args {
    std::string host;
    uint16_t    port{DEFAULT_PORT};
    bool        record{false};
    std::string record_dir{"."};   // directory for output files
};

static void printUsage(const char* prog)
{
    std::cerr
        << "Usage: " << prog
        << " <server-ip> [port] [--record [dir]]\n\n"
        << "  server-ip   IP address or hostname of the Ubuntu server\n"
        << "  port        TCP port (default: " << DEFAULT_PORT << ")\n"
        << "  --record    Save each camera stream to an MP4 file\n"
        << "  dir         Output directory for recordings (default: .)\n\n"
        << "Examples:\n"
        << "  " << prog << " 192.168.1.10\n"
        << "  " << prog << " 192.168.1.10 8554 --record\n"
        << "  " << prog << " 192.168.1.10 8554 --record /tmp/recordings\n";
}

static Args parseArgs(int argc, char* argv[])
{
    Args a;
    if (argc < 2) return a;   // host will be empty → caller prints usage

    a.host = argv[1];

    int i = 2;

    // Optional port (numeric, not starting with '-')
    if (i < argc && argv[i][0] != '-') {
        a.port = static_cast<uint16_t>(std::stoi(argv[i]));
        ++i;
    }

    // Optional --record [dir]
    for (; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--record") {
            a.record = true;
            // If the next token exists and is not a flag, treat it as dir
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                a.record_dir = argv[++i];
            }
        }
    }

    return a;
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    const Args args = parseArgs(argc, argv);

    if (args.host.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    // ── Shared objects ────────────────────────────────────────────────────────
    constexpr int WIDTH  = 640;
    constexpr int HEIGHT = 480;

    Display display(MAX_CAMERAS, WIDTH, HEIGHT);

    std::vector<std::unique_ptr<VideoDecoder>> decoders;
    decoders.reserve(MAX_CAMERAS);
    for (int i = 0; i < MAX_CAMERAS; ++i)
        decoders.push_back(std::make_unique<VideoDecoder>());

    // Recorder is always constructed; it just stays idle unless --record was passed.
    Recorder recorder(args.record_dir);
    if (args.record)
        std::cout << "[main] Recording enabled → " << args.record_dir << "/\n";

    // ── Packet handler (runs on the receive thread) ───────────────────────────
    auto onPacket = [&](ReceivedPacket pkt) {
        if (pkt.cam_id < 0 || pkt.cam_id >= MAX_CAMERAS) return;

        if (pkt.type == PacketType::Init) {
            if (pkt.data.size() < sizeof(InitPayloadHeader)) return;

            const auto* iph = reinterpret_cast<const InitPayloadHeader*>(
                pkt.data.data());
            const int w        = static_cast<int>(ntohl(iph->width));
            const int h        = static_cast<int>(ntohl(iph->height));
            const int fps      = static_cast<int>(ntohl(iph->fps));
            const int extra_sz = static_cast<int>(
                pkt.data.size() - sizeof(InitPayloadHeader));
            const uint8_t* extra = pkt.data.data() + sizeof(InitPayloadHeader);

            if (decoders[pkt.cam_id]->init(w, h, extra, extra_sz)) {
                std::cout << "[main] Decoder ready: cam" << pkt.cam_id
                          << " " << w << "x" << h << " @ " << fps << " fps\n";
            }

            // Open the video file for this camera now that we know its params
            if (args.record)
                recorder.open(pkt.cam_id, w, h, fps);
        }
        else if (pkt.type == PacketType::Video) {
            auto& dec = decoders[pkt.cam_id];
            if (!dec->isInitialized()) return;

            cv::Mat frame = dec->decode(pkt.data.data(),
                                        static_cast<int>(pkt.data.size()));
            if (frame.empty()) return;

            if (args.record)
                recorder.writeFrame(pkt.cam_id, frame);

            display.updateFrame(pkt.cam_id, std::move(frame));
        }
    };

    // ── Connect and start receiving ───────────────────────────────────────────
    StreamReceiver receiver(args.host, args.port);
    receiver.start(std::move(onPacket));

    std::cout << "[main] Connecting to " << args.host << ":" << args.port << " ...\n";

    // Wait up to 5 s for the TCP handshake before entering the display loop
    {
        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::seconds(5);
        while (!receiver.isConnected() && receiver.isRunning()
               && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    if (!receiver.isConnected()) {
        std::cerr << "[main] Could not reach server – check IP and port\n";
        receiver.stop();
        return 1;
    }

    std::cout << "[main] Press ESC or close the window to quit\n";

    // ── Main display loop (must run on the main thread on macOS) ─────────────
    while (g_running && receiver.isConnected()) {
        if (!display.render()) break;
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    std::cout << "[main] Shutting down...\n";
    receiver.stop();
    if (args.record) recorder.close();
    return 0;
}
