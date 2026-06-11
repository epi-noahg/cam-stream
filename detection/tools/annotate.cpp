// camdetect_annotate
// ─────────────────────────────────────────────────────────────────────────────
// The debug viewer, plus ground-truth annotation: link the three videos with
// the per-cam delay sliders, scrub to the interesting moments and record what
// SHOULD happen there.  The result is a YAML test spec that camdetect_runtest
// replays as a unit test.
//
// Extra keys on top of the debug UI:
//   d   expected dart at the current frame (type the zone in the terminal,
//       e.g. T20, D16, 7, 25, Bull, MISS)
//   h   toggle a human-in-frame interval (begin / end)
//   c   board-must-be-cleared marker (round boundary)
//   u   undo last annotation
//   w   write the test spec (also auto-saved on quit)
//
// Usage:
//   camdetect_annotate cam0.mp4 cam1.mp4 cam2.mp4 cam0.yml cam1.yml cam2.yml test.yml

#include "../sources/FileSource.hpp"
#include "TestSpec.hpp"
#include "camdetect/BoardCalibration.hpp"
#include "camdetect/DebugUI.hpp"
#include "camdetect/Pipeline.hpp"
#include "camdetect/Types.hpp"
#include "camdetect/ZoneMap.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <opencv2/highgui.hpp>
#include <sstream>

using namespace camdetect;

namespace {

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

// True if an odd number of human_begin markers precede the end of the event
// list, i.e. the last opened interval has not been closed yet.
bool humanIntervalOpen(const std::vector<TestEvent>& events)
{
    int depth = 0;
    for (const auto& e : events) {
        if (e.type == TestEvent::Type::HumanBegin) ++depth;
        if (e.type == TestEvent::Type::HumanEnd)   --depth;
    }
    return depth > 0;
}

std::string describe(const TestEvent& e)
{
    std::ostringstream s;
    switch (e.type) {
        case TestEvent::Type::Dart:
            s << "dart " << e.zone << " (" << e.score << ")"; break;
        case TestEvent::Type::HumanBegin: s << "human begin"; break;
        case TestEvent::Type::HumanEnd:   s << "human end";   break;
        case TestEvent::Type::Clear:      s << "clear";       break;
    }
    s << " @" << e.frame;
    return s.str();
}

} // anon

int main(int argc, char* argv[])
{
    if (argc < 2 + 2 * NUM_CAMS) {
        std::cerr << "Usage: " << argv[0]
                  << " cam0.mp4 cam1.mp4 cam2.mp4"
                  << " cam0.yml cam1.yml cam2.yml test.yml\n";
        return 1;
    }
    const std::string spec_path = argv[1 + 2 * NUM_CAMS];

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

    // ── Test spec: resume an existing file or start fresh ──────────────────
    TestSpec spec;
    bool     modified = false;
    if (spec.load(spec_path)) {
        std::cout << "[annotate] resuming " << spec_path << " ("
                  << spec.events.size() << " events)\n";
    } else {
        std::cout << "[annotate] new test spec: " << spec_path << '\n';
    }
    spec.fps = std::max(1, sources[0]->fps());
    for (int i = 0; i < NUM_CAMS; ++i) {
        spec.videos[i] = argv[1 + i];
        spec.calibs[i] = argv[1 + NUM_CAMS + i];
    }

    const int tile_w = sources[0]->width();
    const int tile_h = sources[0]->height();

    Pipeline pipeline(calibs);
    DebugUI  ui(calibs, tile_w, tile_h, 480);

    for (int i = 0; i < NUM_CAMS; ++i) {
        ZoneMap zm;
        const std::string zpath = ZoneMap::companionPath(argv[1 + NUM_CAMS + i]);
        if (zm.loadFromFile(zpath)) {
            ui.setZoneMap(i, zm);
            pipeline.setZoneMap(i, std::move(zm));
            std::cout << "[cam" << i << "] pixel zone map: " << zpath << '\n';
        }
    }

    int round_total_score = 0;
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

    // ── Trackbars ──────────────────────────────────────────────────────────
    SeekState seek{};
    cv::createTrackbar("frame", DebugUI::WINDOW_NAME, nullptr,
                       std::max(1, min_total_frames - 1),
                       onSeekTrackbar, &seek);

    int threshold_slider = 15;
    cv::createTrackbar("threshold", DebugUI::WINDOW_NAME,
                       &threshold_slider, 80);

    std::array<int, NUM_CAMS> delay_sliders{};
    for (int i = 0; i < NUM_CAMS; ++i) {
        delay_sliders[i] = DELAY_TRACKBAR_CENTER +
                           std::clamp(spec.offsets[i],
                                      -DELAY_TRACKBAR_CENTER,
                                      DELAY_TRACKBAR_CENTER);
        const std::string name = "delay_cam" + std::to_string(i);
        cv::createTrackbar(name, DebugUI::WINDOW_NAME,
                           &delay_sliders[i], DELAY_TRACKBAR_MAX);
    }

    // ── State ──────────────────────────────────────────────────────────────
    std::array<cv::Mat, NUM_CAMS> last_frames;
    std::array<int,     NUM_CAMS> delays{};
    for (int i = 0; i < NUM_CAMS; ++i) {
        delays[i] = delay_sliders[i] - DELAY_TRACKBAR_CENTER;
        ui.setCamDelay(i, delays[i]);
    }
    int master_pos = -1;

    auto camTarget = [&](int i) {
        const int t = master_pos + delays[i];
        return std::clamp(t, 0, std::max(0, sources[i]->totalFrames() - 1));
    };

    const int sync_fps = std::max(1, sources[0]->fps());
    auto masterTs = [&]() { return double(master_pos) / double(sync_fps); };

    auto readCam = [&](int i, int target) -> bool {
        if (sources[i]->currentFrame() + 1 != target) {
            sources[i]->seek(target);
        }
        cv::Mat f;
        double  source_ts = 0.0;
        if (!sources[i]->next(f, source_ts)) return false;
        pipeline.feedFrame(i, f, masterTs());
        last_frames[i] = f;
        return true;
    };

    auto syncAllCamsAtMaster = [&]() {
        bool any = false;
        for (int i = 0; i < NUM_CAMS; ++i)
            any |= readCam(i, camTarget(i));
        seek.last_seen = master_pos;
        cv::setTrackbarPos("frame", DebugUI::WINDOW_NAME, master_pos);
        return any;
    };

    // ── Annotation actions ──────────────────────────────────────────────────
    auto saveSpec = [&]() {
        for (int i = 0; i < NUM_CAMS; ++i) spec.offsets[i] = delays[i];
        if (spec.save(spec_path)) {
            modified = false;
            std::cout << "[annotate] saved " << spec.events.size()
                      << " events -> " << spec_path << '\n';
        } else {
            std::cerr << "[annotate] FAILED to write " << spec_path << '\n';
        }
    };

    auto addEvent = [&](TestEvent e) {
        std::cout << "[annotate] + " << describe(e) << '\n';
        spec.events.push_back(std::move(e));
        modified = true;
    };

    // Pauses playback and reads a zone label from the terminal.
    auto promptDart = [&]() {
        std::cout << "\n[annotate] dart @" << master_pos
                  << " — zone (T20/D16/7/25/Bull/MISS, empty = cancel): "
                  << std::flush;
        std::string line;
        std::getline(std::cin, line);
        const std::string zone = canonicalZone(line);
        if (zone.empty()) {
            std::cout << "[annotate] cancelled"
                      << (line.empty() ? "" : " (invalid zone '" + line + "')")
                      << '\n';
            return;
        }
        TestEvent e;
        e.type  = TestEvent::Type::Dart;
        e.frame = master_pos;
        e.zone  = zone;
        e.score = zoneScore(zone);
        addEvent(std::move(e));
    };

    auto annotatePanel = [&]() {
        int darts = 0, clears = 0, humans = 0;
        for (const auto& e : spec.events) {
            if (e.type == TestEvent::Type::Dart)       ++darts;
            if (e.type == TestEvent::Type::Clear)      ++clears;
            if (e.type == TestEvent::Type::HumanBegin) ++humans;
        }
        std::vector<std::string> lines;
        {
            const auto slash = spec_path.find_last_of('/');
            lines.push_back("  file   " + spec_path.substr(slash + 1) +
                            (modified ? " *" : ""));
        }
        std::ostringstream s;
        s << "  events " << darts << " darts, " << humans << " human, "
          << clears << " clear";
        lines.push_back(s.str());
        if (humanIntervalOpen(spec.events))
            lines.push_back("  human  OPEN ('h' to close)");
        if (!spec.events.empty())
            lines.push_back("  last   " + describe(spec.events.back()));
        // Events near the playhead, so scrubbing shows what's annotated here.
        for (const auto& e : spec.events) {
            if (std::abs(e.frame - master_pos) <= 45)
                lines.push_back("  here   " + describe(e));
        }
        lines.push_back("  keys   d/h/c dart/human/clear");
        lines.push_back("         u undo   w write");
        ui.setExtraPanel("ANNOTATE", lines);
    };

    master_pos = 0;
    syncAllCamsAtMaster();

    bool running = true;
    while (running) {
        if (std::abs(static_cast<float>(threshold_slider) -
                     pipeline.diffThreshold()) > 0.5f) {
            pipeline.setDiffThreshold(static_cast<float>(threshold_slider));
        }
        ui.setDiffThreshold(static_cast<float>(threshold_slider));

        for (int i = 0; i < NUM_CAMS; ++i) {
            const int new_delay = delay_sliders[i] - DELAY_TRACKBAR_CENTER;
            if (new_delay != delays[i]) {
                delays[i] = new_delay;
                readCam(i, camTarget(i));
                ui.setCamDelay(i, new_delay);
                modified = true;
            }
        }

        if (seek.user_dragged) {
            seek.user_dragged = false;
            master_pos = seek.position;
            pipeline.resetRound();
            pipeline.refreshBackground();
            syncAllCamsAtMaster();
        }

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
                    std::cout << "[annotate] End of streams.\n";
                    break;
                }
            }
            // At the end of the streams keep the UI alive (unlike the debug
            // viewer) so the tail of the session can still be annotated.
        }

        for (int i = 0; i < NUM_CAMS; ++i) {
            if (!last_frames[i].empty())
                ui.updateCam(i, last_frames[i], pipeline.camViz(i));
            ui.setCamDelay(i, delays[i]);
        }
        recomputeTotal();
        ui.setRoundProgress(pipeline.dartsInRound(), round_total_score);
        ui.setRoundHits(pipeline.roundHits());
        const RoundStatus rs = pipeline.roundStatus();
        ui.setRoundStatus(rs.message, static_cast<int>(rs.phase));
        annotatePanel();

        if (ui.consumeResetRequest())     pipeline.resetRound();
        if (ui.consumeBgRefreshRequest()) pipeline.refreshBackground();

        if (!ui.render()) running = false;

        switch (ui.consumeKey()) {
            case 'd':
                promptDart();
                break;
            case 'h': {
                TestEvent e;
                e.type  = humanIntervalOpen(spec.events)
                              ? TestEvent::Type::HumanEnd
                              : TestEvent::Type::HumanBegin;
                e.frame = master_pos;
                addEvent(std::move(e));
                break;
            }
            case 'c': {
                TestEvent e;
                e.type  = TestEvent::Type::Clear;
                e.frame = master_pos;
                addEvent(std::move(e));
                break;
            }
            case 'u':
                if (!spec.events.empty()) {
                    std::cout << "[annotate] - "
                              << describe(spec.events.back()) << '\n';
                    spec.events.pop_back();
                    modified = true;
                }
                break;
            case 'w':
                saveSpec();
                break;
            default: break;
        }
    }

    if (modified) saveSpec();
    return 0;
}
