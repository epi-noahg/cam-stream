// ─────────────────────────────────────────────────────────────────────────────
// dartserver — authoritative darts game server (Phase 0/2 skeleton).
//
// Wires camera/replay frames → detection → game and logs the results.  The
// WebSocket API (Phase 3) and SQLite persistence (Phase 4) plug into the same
// GameManager + DetectionService instances created here.
//
// Usage:
//   dartserver --replay v0.mp4 v1.mp4 v2.mp4 cam0.yml cam1.yml cam2.yml
//   dartserver --live [dev0 dev1 dev2] cam0.yml cam1.yml cam2.yml
// ─────────────────────────────────────────────────────────────────────────────

#include "api/Dispatcher.hpp"
#include "api/WsServer.hpp"
#include "detection/DetectionService.hpp"
#include "game/GameManager.hpp"
#include "persistence/Db.hpp"

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace dart;

namespace {
std::atomic<bool> g_running{true};
void onSignal(int) { g_running = false; }

void usage(const char* p) {
    std::cerr
        << "Usage:\n"
        << "  " << p << " --replay v0 v1 v2 cam0.yml cam1.yml cam2.yml [--loop] [--offsets a,b,c] [--window]\n"
        << "  " << p << " --live [dev0 dev1 dev2] cam0.yml cam1.yml cam2.yml\n"
        << "  --window  show a local pause/play/scrub control window (replay only)\n";
}

// Trackbar drag → seek. Ignore tiny diffs so our own setTrackbarPos (used to
// reflect playback progress) doesn't feed back into a seek loop.
void onTrackbar(int pos, void* userdata) {
    auto* det = static_cast<detect::DetectionService*>(userdata);
    int d = pos - det->replayPos();
    if (d < 0) d = -d;
    if (d > 1) det->replaySeek(pos);
}

// Local control window (Mac). Runs on the main thread (macOS HighGUI rule).
void runReplayWindow(detect::DetectionService& det) {
    const std::string win = "dartserver - replay";
    cv::namedWindow(win, cv::WINDOW_AUTOSIZE);
    const int total = det.replayTotal() > 0 ? det.replayTotal() : 1;
    cv::createTrackbar("pos", win, nullptr, total, onTrackbar, &det);
    std::cout << "[replay] window controls: space=play/pause  d=step  "
                 "a=-1  j=-1s  l=+1s  q=quit\n";

    while (g_running) {
        cv::Mat montage;
        if (det.replaySnapshot(montage, 1100)) {
            const int pos = det.replayPos();
            const std::string label =
                (det.replayPaused() ? "PAUSE  " : "PLAY   ") +
                std::to_string(pos) + " / " + std::to_string(det.replayTotal());
            cv::putText(montage, label, {12, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.9,
                        {0, 255, 0}, 2);
            cv::imshow(win, montage);
            cv::setTrackbarPos("pos", win, pos < total ? pos : total);
        }
        const int key = cv::waitKey(30) & 0xFF;
        switch (key) {
            case ' ': det.replayTogglePause(); break;
            case 'd': det.replayStep(1); break;
            case 'a': det.replaySeek(det.replayPos() - 1); break;
            case 'l': det.replaySeek(det.replayPos() + 30); break;
            case 'j': det.replaySeek(det.replayPos() - 30); break;
            case 'q':
            case 27: g_running = false; break;
            default: break;
        }
    }
    cv::destroyAllWindows();
}
} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) { usage(argv[0]); return 1; }

    detect::DetectionService::Config cfg;

    // Split flags from positional args. Flags: --loop, --offsets a,b,c, --window.
    bool showWindow = false;
    std::vector<std::string> pos;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--loop") {
            cfg.loop = true;
        } else if (args[i] == "--window") {
            showWindow = true;
        } else if (args[i] == "--offsets" && i + 1 < args.size()) {
            const std::string& o = args[++i];   // "a,b,c"
            std::size_t p = 0;
            for (int c = 0; c < 3 && p <= o.size(); ++c) {
                cfg.offsets[c] = std::atoi(o.c_str() + p);
                p = o.find(',', p);
                if (p == std::string::npos) break;
                ++p;
            }
        } else {
            pos.push_back(args[i]);
        }
    }
    if (pos.empty()) { usage(argv[0]); return 1; }
    const std::string mode = pos[0];

    if (mode == "--replay") {
        if (pos.size() != 1 + 3 + 3) { usage(argv[0]); return 1; }
        cfg.replay = true;
        for (int c = 0; c < 3; ++c) cfg.videoPaths[c] = pos[1 + c];
        for (int c = 0; c < 3; ++c) cfg.calibPaths[c] = pos[4 + c];
    } else if (mode == "--live") {
        if (pos.size() == 1 + 3) {
            for (int c = 0; c < 3; ++c) cfg.calibPaths[c] = pos[1 + c];
        } else if (pos.size() == 1 + 6) {
            for (int c = 0; c < 3; ++c) cfg.devices[c]    = pos[1 + c];
            for (int c = 0; c < 3; ++c) cfg.calibPaths[c] = pos[4 + c];
        } else { usage(argv[0]); return 1; }
    } else {
        usage(argv[0]);
        return 1;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    // No game is created up front: the tablet creates one via `create_game`
    // (the Setup screen).  Darts detected before a game exists are ignored.
    game::GameManager gm;

    detect::DetectionService det(gm);

    // ── Persistence ──────────────────────────────────────────────────────
    persist::Db db;
    if (!db.open("dartserver.db"))
        std::cerr << "[main] WARNING: persistence disabled (db open failed)\n";

    // ── WebSocket API: broadcasts state, accepts commands/corrections ────
    constexpr std::uint16_t WS_PORT = 8080;
    api::WsServer   ws(WS_PORT);
    api::Dispatcher dispatcher(ws, gm, det, &db);
    dispatcher.wire();   // installs gm/det/ws callbacks + persistence

    if (!det.init(cfg)) {
        std::cerr << "[main] detection init failed\n";
        return 1;
    }
    std::cout << "[main] starting (" << mode << "), WS on " << WS_PORT << "\n";
    ws.start();
    det.start();

    if (cfg.replay && showWindow) {
        // Local transport window on the Mac (main thread for macOS HighGUI).
        runReplayWindow(det);
    } else {
        // Stay alive until Ctrl+C even after a replay finishes, so the tablet
        // remains connected and can review / correct the final state.
        while (g_running)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[main] shutting down...\n";
    det.stop();
    ws.stop();
    return 0;
}
