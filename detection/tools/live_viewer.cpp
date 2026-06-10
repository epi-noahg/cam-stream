// camdetect_live
// ─────────────────────────────────────────────────────────────────────────────
// Live dart detection over the camstream TCP protocol.  Reuses the client's
// StreamReceiver + VideoDecoder to receive H.264 packets from the server,
// decodes to BGR, and feeds the frames into the same Pipeline + DebugUI used
// by the offline debug tool.
//
// Usage:
//   camdetect_live <server-ip> [port] cam0.yml cam1.yml cam2.yml

#include "camdetect/BoardCalibration.hpp"
#include "camdetect/DebugUI.hpp"
#include "camdetect/Pipeline.hpp"
#include "camdetect/Types.hpp"
#include "camdetect/ZoneMap.hpp"

#include "Protocol.hpp"
#include "StreamReceiver.hpp"
#include "VideoDecoder.hpp"

#include <arpa/inet.h>

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace camdetect;

namespace {

std::atomic<bool> g_running{true};
void onSignal(int) { g_running = false; }

bool isNumeric(const char* s)
{
    if (!s || !*s) return false;
    for (const char* p = s; *p; ++p)
        if (*p < '0' || *p > '9') return false;
    return true;
}

void printUsage(const char* prog)
{
    std::cerr
        << "Usage: " << prog
        << " <server-ip> [port] cam0.yml cam1.yml cam2.yml\n"
        << "  server-ip   IP / hostname of the camstream server\n"
        << "  port        TCP port (default: "
        << camstream::DEFAULT_PORT << ")\n"
        << "  camN.yml    Per-camera board calibration\n";
}

} // anon

int main(int argc, char* argv[])
{
    if (argc < 1 + 1 + NUM_CAMS) {
        printUsage(argv[0]);
        return 1;
    }

    // ── Parse args: <host> [port] cam0.yml cam1.yml cam2.yml ──────────────
    const std::string host = argv[1];
    uint16_t          port = camstream::DEFAULT_PORT;
    int               i    = 2;
    if (i < argc && isNumeric(argv[i])) {
        port = static_cast<uint16_t>(std::stoi(argv[i]));
        ++i;
    }
    if (argc - i < NUM_CAMS) {
        printUsage(argv[0]);
        return 1;
    }

    std::array<BoardCalibration, NUM_CAMS> calibs;
    for (int c = 0; c < NUM_CAMS; ++c) {
        if (!calibs[c].loadFromFile(argv[i + c])) {
            std::cerr << "Failed to load calibration: " << argv[i + c] << '\n';
            return 1;
        }
    }

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    Pipeline pipeline(calibs);

    int round_total_score = 0;   // recomputed from pipeline.roundHits() each tick
    pipeline.setOnHit([&](const FusedHit& h) {
        std::cout << "[hit] t=" << h.timestamp
                  << "  zone=" << h.zone
                  << "  score=" << h.score
                  << "  conf=" << h.confidence << '\n';
    });
    auto recomputeTotal = [&]() {
        int s = 0;
        for (const auto& h : pipeline.roundHits()) s += h.score;
        round_total_score = s;
    };

    // ── Decoders + shared transport between recv thread and UI thread ─────
    std::array<std::unique_ptr<camstream::VideoDecoder>, NUM_CAMS> decoders;
    for (auto& d : decoders) d = std::make_unique<camstream::VideoDecoder>();

    std::mutex                  frame_mtx;
    std::array<cv::Mat, NUM_CAMS> latest_frames;

    // Camera tile dims are fixed: the current server config streams 640x480.
    // The DebugUI uses cv::WINDOW_AUTOSIZE so we can't easily resize it after
    // creation — if the streams ever come in at a different resolution we
    // letterbox-fit them into these tiles.
    constexpr int TILE_W = 640;
    constexpr int TILE_H = 480;
    DebugUI ui(calibs, TILE_W, TILE_H, 480);

    // Pixel-accurate zone maps (camN_zones.png next to camN.yml), if present:
    // scoring reads them at the tip pixel, and 'z' overlays them on the tiles.
    for (int c = 0; c < NUM_CAMS; ++c) {
        ZoneMap zm;
        const std::string zpath = ZoneMap::companionPath(argv[i + c]);
        if (zm.loadFromFile(zpath)) {
            ui.setZoneMap(c, zm);
            pipeline.setZoneMap(c, std::move(zm));
            std::cout << "[cam" << c << "] pixel zone map: " << zpath << '\n';
        }
    }

    const auto session_start = std::chrono::steady_clock::now();
    auto monotonicTs = [&]() {
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now() - session_start).count();
    };

    auto onPacket = [&](camstream::ReceivedPacket pkt) {
        if (pkt.cam_id < 0 || pkt.cam_id >= NUM_CAMS) return;

        if (pkt.type == camstream::PacketType::Init) {
            if (pkt.data.size() < sizeof(camstream::InitPayloadHeader)) return;
            const auto* iph = reinterpret_cast<
                const camstream::InitPayloadHeader*>(pkt.data.data());
            const int w        = static_cast<int>(ntohl(iph->width));
            const int h        = static_cast<int>(ntohl(iph->height));
            const int fps      = static_cast<int>(ntohl(iph->fps));
            const int extra_sz = static_cast<int>(
                pkt.data.size() - sizeof(camstream::InitPayloadHeader));
            const uint8_t* extra =
                pkt.data.data() + sizeof(camstream::InitPayloadHeader);
            if (decoders[pkt.cam_id]->init(w, h, extra, extra_sz)) {
                std::cout << "[live] cam" << pkt.cam_id
                          << " ready: " << w << "x" << h
                          << " @ " << fps << " fps\n";
            }
            return;
        }

        if (pkt.type != camstream::PacketType::Video) return;
        auto& dec = decoders[pkt.cam_id];
        if (!dec->isInitialized()) return;

        cv::Mat frame = dec->decode(pkt.data.data(),
                                    static_cast<int>(pkt.data.size()));
        if (frame.empty()) return;

        // Shared monotonic clock so fusion groups votes by physical time.
        const double ts = monotonicTs();
        pipeline.feedFrame(pkt.cam_id, frame, ts);

        std::lock_guard<std::mutex> lk(frame_mtx);
        latest_frames[pkt.cam_id] = std::move(frame);
    };

    camstream::StreamReceiver receiver(host, port);
    receiver.start(std::move(onPacket));

    std::cout << "[live] Connecting to " << host << ":" << port << " ...\n";
    {
        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::seconds(5);
        while (!receiver.isConnected() && receiver.isRunning()
               && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    if (!receiver.isConnected()) {
        std::cerr << "[live] Could not reach server – check IP/port\n";
        receiver.stop();
        return 1;
    }
    std::cout << "[live] Connected.  q/ESC to quit.\n";

    // ── Main UI loop on the main thread (macOS requirement) ───────────────
    while (g_running && receiver.isConnected()) {
        // Snapshot the latest decoded frame from each cam.  cv::Mat is
        // ref-counted so the copy under the lock is cheap.
        std::array<cv::Mat, NUM_CAMS> local;
        {
            std::lock_guard<std::mutex> lk(frame_mtx);
            for (int c = 0; c < NUM_CAMS; ++c)
                if (!latest_frames[c].empty()) local[c] = latest_frames[c];
        }
        for (int c = 0; c < NUM_CAMS; ++c)
            if (!local[c].empty())
                ui.updateCam(c, local[c], pipeline.camViz(c));

        recomputeTotal();
        ui.setRoundProgress(pipeline.dartsInRound(), round_total_score);
        ui.setRoundHits(pipeline.roundHits());
        const RoundStatus rs = pipeline.roundStatus();
        ui.setRoundStatus(rs.message, static_cast<int>(rs.phase));

        if (ui.consumeResetRequest()) {
            pipeline.resetRound();
        }
        if (ui.consumeBgRefreshRequest()) {
            pipeline.refreshBackground();
        }
        // Step / scrub controls have no meaning live → just consume + ignore.
        ui.consumeStepForward();
        ui.consumeStepBackward();

        if (!ui.render()) break;
    }

    std::cout << "[live] Shutting down...\n";
    receiver.stop();
    return 0;
}
