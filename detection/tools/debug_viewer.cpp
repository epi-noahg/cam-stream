// camdetect_debug
// ─────────────────────────────────────────────────────────────────────────────
// Replays 3 video files through Pipeline and shows the live debug UI.
//
// Usage:
//   camdetect_debug cam0.mp4 cam1.mp4 cam2.mp4 cam0.yml cam1.yml cam2.yml

#include "../sources/FileSource.hpp"
#include "camdetect/BoardCalibration.hpp"
#include "camdetect/DebugUI.hpp"
#include "camdetect/Pipeline.hpp"
#include "camdetect/Types.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <opencv2/highgui.hpp>

using namespace camdetect;

namespace {

// Trackbars are unsigned in HighGUI; we offset delay sliders by this value so
// the user-facing range is symmetric around 0.
constexpr int DELAY_TRACKBAR_MAX    = 120;
constexpr int DELAY_TRACKBAR_CENTER = 60;

struct SeekState {
    int  position {0};
    int  last_seen{0};
    bool user_dragged{false};
};

void onSeekTrackbar(int pos, void* userdata)
{
    auto* st = static_cast<SeekState*>(userdata);
    if (pos != st->last_seen) {
        st->position    = pos;
        st->user_dragged = true;
    }
}

} // anon

int main(int argc, char* argv[])
{
    if (argc < 1 + 2 * NUM_CAMS) {
        std::cerr << "Usage: " << argv[0]
                  << " cam0.mp4 cam1.mp4 cam2.mp4"
                  << " cam0.yml cam1.yml cam2.yml\n";
        return 1;
    }

    std::array<BoardCalibration, NUM_CAMS> calibs;
    for (int i = 0; i < NUM_CAMS; ++i) {
        if (!calibs[i].loadFromFile(argv[1 + NUM_CAMS + i])) {
            std::cerr << "Failed to load calibration: "
                      << argv[1 + NUM_CAMS + i] << '\n';
            return 1;
        }
    }

    std::array<std::unique_ptr<FileSource>, NUM_CAMS> sources;
    int min_total_frames = INT32_MAX;
    for (int i = 0; i < NUM_CAMS; ++i) {
        sources[i] = std::make_unique<FileSource>(argv[1 + i]);
        if (!sources[i]->isOpen()) {
            std::cerr << "Failed to open video: " << argv[1 + i] << '\n';
            return 1;
        }
        min_total_frames = std::min(min_total_frames, sources[i]->totalFrames());
    }
    if (min_total_frames == INT32_MAX) min_total_frames = 0;

    const int tile_w = sources[0]->width();
    const int tile_h = sources[0]->height();

    Pipeline pipeline(calibs);
    DebugUI  ui(calibs, tile_w, tile_h, 480);

    int round_total_score = 0;
    pipeline.setOnHit([&](const FusedHit& h) {
        round_total_score += h.score;
        std::cout << "[hit] t=" << h.timestamp
                  << "  zone=" << h.zone
                  << "  score=" << h.score
                  << "  conf=" << h.confidence << '\n';
    });

    // ── Trackbars ──────────────────────────────────────────────────────────
    SeekState seek{};
    cv::createTrackbar("frame", DebugUI::WINDOW_NAME, nullptr,
                       std::max(1, min_total_frames - 1),
                       onSeekTrackbar, &seek);

    int threshold_slider = 15;
    cv::createTrackbar("threshold", DebugUI::WINDOW_NAME,
                       &threshold_slider, 80);

    int line_merge_slider = 6;
    cv::createTrackbar("line_merge_px", DebugUI::WINDOW_NAME,
                       &line_merge_slider, 30);

    std::array<int, NUM_CAMS> delay_sliders{};
    for (int i = 0; i < NUM_CAMS; ++i) {
        delay_sliders[i] = DELAY_TRACKBAR_CENTER;
        const std::string name = "delay_cam" + std::to_string(i);
        cv::createTrackbar(name, DebugUI::WINDOW_NAME,
                           &delay_sliders[i], DELAY_TRACKBAR_MAX);
    }

    // ── State ──────────────────────────────────────────────────────────────
    std::array<cv::Mat, NUM_CAMS> last_frames;
    std::array<int,     NUM_CAMS> delays{};   // current per-cam offsets
    int master_pos = -1;                       // last shown master frame index

    auto camTarget = [&](int i) {
        const int t = master_pos + delays[i];
        return std::clamp(t, 0, std::max(0, sources[i]->totalFrames() - 1));
    };

    // Reads one frame from cam `i`, seeking first if its current head doesn't
    // already match `target`.  Updates last_frames + feeds the pipeline.
    auto readCam = [&](int i, int target) -> bool {
        if (sources[i]->currentFrame() + 1 != target) {
            sources[i]->seek(target);
        }
        cv::Mat f;
        double  ts = 0.0;
        if (!sources[i]->next(f, ts)) return false;
        pipeline.feedFrame(i, f, ts);
        last_frames[i] = f;
        return true;
    };

    // Synchronously re-render all cams at the current master position.  Used
    // on initial frame, scrub, step, or any delay change.
    auto syncAllCamsAtMaster = [&]() {
        bool any = false;
        for (int i = 0; i < NUM_CAMS; ++i)
            any |= readCam(i, camTarget(i));
        seek.last_seen = master_pos;
        cv::setTrackbarPos("frame", DebugUI::WINDOW_NAME, master_pos);
        return any;
    };

    // Initial frame so the UI has something to show before the user hits play.
    master_pos = 0;
    syncAllCamsAtMaster();

    bool running = true;
    while (running) {
        // ── Apply live tunables that don't require a re-seek ───────────────
        if (std::abs(static_cast<float>(threshold_slider) -
                     pipeline.diffThreshold()) > 0.5f) {
            pipeline.setDiffThreshold(static_cast<float>(threshold_slider));
        }
        if (std::abs(static_cast<float>(line_merge_slider) -
                     pipeline.lineMergePerpPx()) > 0.5f) {
            pipeline.setLineMergePerpPx(static_cast<float>(line_merge_slider));
        }
        ui.setDiffThreshold(static_cast<float>(threshold_slider));

        // ── Per-cam delay sliders → live re-seek of the affected cam ───────
        for (int i = 0; i < NUM_CAMS; ++i) {
            const int new_delay = delay_sliders[i] - DELAY_TRACKBAR_CENTER;
            if (new_delay != delays[i]) {
                delays[i] = new_delay;
                readCam(i, camTarget(i));
                ui.setCamDelay(i, new_delay);
            }
        }

        // ── User dragged the scrub bar: re-sync everything at the new pos ──
        if (seek.user_dragged) {
            seek.user_dragged = false;
            master_pos = seek.position;
            pipeline.resetRound();
            pipeline.refreshBackground();
            round_total_score = 0;
            syncAllCamsAtMaster();
        }

        // ── Step / play ────────────────────────────────────────────────────
        const bool paused   = ui.isPaused();
        const bool step_fwd = ui.consumeStepForward();
        const bool step_bwd = ui.consumeStepBackward();

        if (step_bwd) {
            master_pos = std::max(0, master_pos - 1);
            syncAllCamsAtMaster();
        } else if (!paused || step_fwd) {
            if (master_pos < min_total_frames - 1) {
                ++master_pos;
                if (!syncAllCamsAtMaster()) {
                    std::cout << "[debug] End of streams.\n";
                    running = false;
                    break;
                }
            } else if (!paused) {
                std::cout << "[debug] End of streams.\n";
                running = false;
                break;
            }
        }

        // ── Push the latest viz into the UI ────────────────────────────────
        for (int i = 0; i < NUM_CAMS; ++i) {
            if (!last_frames[i].empty())
                ui.updateCam(i, last_frames[i], pipeline.camViz(i));
            ui.setCamDelay(i, delays[i]);
        }
        ui.setRoundProgress(pipeline.dartsInRound(), round_total_score);
        ui.setRoundHits(pipeline.roundHits());

        if (ui.consumeResetRequest()) {
            pipeline.resetRound();
            round_total_score = 0;
        }
        if (ui.consumeBgRefreshRequest()) {
            pipeline.refreshBackground();
        }

        if (!ui.render()) running = false;
    }
    return 0;
}
