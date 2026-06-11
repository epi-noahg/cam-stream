// camdetect_runtest
// ─────────────────────────────────────────────────────────────────────────────
// Replays a recorded session through the Pipeline with the per-cam offsets
// from a test spec (written by camdetect_annotate) and asserts the fused hits
// match the annotated ground truth:
//
//   * every annotated dart must produce a fused hit with the same zone within
//     ±tolerance frames, inside the same round (clear markers are boundaries)
//   * every fused hit must correspond to an annotated dart (no phantom hits,
//     e.g. while a human is in frame)
//
// Exit code 0 = all checks pass, 1 = any failure — so each spec file is a
// unit test runnable from a script or CI.
//
// Usage:
//   camdetect_runtest test.yml [--tol FRAMES]
//   camdetect_runtest test.yml cam0.mp4 cam1.mp4 cam2.mp4 cam0.yml cam1.yml cam2.yml [--tol FRAMES]
//
// Without explicit video/calib args, the paths stored in the spec are used.

#include "../sources/FileSource.hpp"
#include "TestSpec.hpp"
#include "camdetect/BoardCalibration.hpp"
#include "camdetect/Pipeline.hpp"
#include "camdetect/Types.hpp"
#include "camdetect/ZoneMap.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace camdetect;

namespace {

constexpr int DEFAULT_TOLERANCE_FRAMES = 60;   // ±2s @ 30fps

struct RecordedHit {
    int         frame{0};
    std::string zone;
    int         score{0};
    float       confidence{0.f};
    bool        matched{false};
};

struct ExpectedDart {
    TestEvent ev;
    int       round{0};
    bool      matched{false};
};

// Round index for a frame given the sorted clear markers.
int roundOf(int frame, const std::vector<int>& clears)
{
    return static_cast<int>(
        std::upper_bound(clears.begin(), clears.end(), frame) -
        clears.begin());
}

} // anon

int main(int argc, char* argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);

    int tolerance = DEFAULT_TOLERANCE_FRAMES;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--tol" && i + 1 < args.size()) {
            tolerance = std::stoi(args[i + 1]);
            args.erase(args.begin() + i, args.begin() + i + 2);
            break;
        }
    }

    if (args.size() != 1 && args.size() != 1 + 2 * NUM_CAMS) {
        std::cerr << "Usage: " << argv[0] << " test.yml"
                  << " [cam0.mp4 cam1.mp4 cam2.mp4 cam0.yml cam1.yml cam2.yml]"
                  << " [--tol FRAMES]\n";
        return 1;
    }

    TestSpec spec;
    if (!spec.load(args[0])) {
        std::cerr << "Failed to load test spec: " << args[0] << '\n';
        return 1;
    }

    std::array<std::string, NUM_CAMS> video_paths = spec.videos;
    std::array<std::string, NUM_CAMS> calib_paths = spec.calibs;
    if (args.size() == 1 + 2 * NUM_CAMS) {
        for (int i = 0; i < NUM_CAMS; ++i) {
            video_paths[i] = args[1 + i];
            calib_paths[i] = args[1 + NUM_CAMS + i];
        }
    }

    std::array<BoardCalibration, NUM_CAMS> calibs;
    std::array<std::unique_ptr<FileSource>, NUM_CAMS> sources;
    for (int i = 0; i < NUM_CAMS; ++i) {
        if (!calibs[i].loadFromFile(calib_paths[i])) {
            std::cerr << "Failed to load calibration: " << calib_paths[i] << '\n';
            return 1;
        }
        sources[i] = std::make_unique<FileSource>(video_paths[i]);
        if (!sources[i]->isOpen()) {
            std::cerr << "Failed to open video: " << video_paths[i] << '\n';
            return 1;
        }
    }

    Pipeline pipeline(calibs);
    for (int i = 0; i < NUM_CAMS; ++i) {
        ZoneMap zm;
        if (zm.loadFromFile(ZoneMap::companionPath(calib_paths[i])))
            pipeline.setZoneMap(i, std::move(zm));
    }

    const int fps = spec.fps > 0 ? spec.fps : std::max(1, sources[0]->fps());

    std::vector<RecordedHit> hits;
    pipeline.setOnHit([&](const FusedHit& h) {
        RecordedHit r;
        r.frame      = static_cast<int>(h.timestamp * fps + 0.5);
        r.zone       = h.zone;
        r.score      = h.score;
        r.confidence = h.confidence;
        hits.push_back(std::move(r));
    });
    // Late votes can refine an already-emitted hit (same timestamp): amend
    // the recorded entry so the assertion sees the final zone, exactly as a
    // live scoreboard would correct itself.
    pipeline.setOnHitUpdated([&](const FusedHit& h) {
        const int frame = static_cast<int>(h.timestamp * fps + 0.5);
        for (auto& r : hits) {
            if (r.frame != frame) continue;
            r.zone       = h.zone;
            r.score      = h.score;
            r.confidence = h.confidence;
        }
    });

    // ── Replay with the spec's per-cam offsets, master-clock timestamps ─────
    int min_total_frames = INT32_MAX;
    for (int i = 0; i < NUM_CAMS; ++i)
        min_total_frames = std::min(min_total_frames, sources[i]->totalFrames());
    if (min_total_frames == INT32_MAX) min_total_frames = 0;

    std::cout << "[runtest] " << args[0] << "  offsets=[";
    for (int i = 0; i < NUM_CAMS; ++i)
        std::cout << spec.offsets[i] << (i + 1 < NUM_CAMS ? "," : "");
    std::cout << "]  frames=" << min_total_frames
              << "  tol=" << tolerance << "f\n";

    for (int master = 0; master < min_total_frames; ++master) {
        const double ts = double(master) / double(fps);
        for (int i = 0; i < NUM_CAMS; ++i) {
            const int target =
                std::clamp(master + spec.offsets[i],
                           0, std::max(0, sources[i]->totalFrames() - 1));
            if (sources[i]->currentFrame() + 1 != target)
                sources[i]->seek(target);
            cv::Mat frame;
            double  source_ts = 0.0;
            if (sources[i]->next(frame, source_ts))
                pipeline.feedFrame(i, frame, ts);
        }
    }

    // ── Match hits against the annotated darts ──────────────────────────────
    std::vector<int> clears;
    std::vector<ExpectedDart> expected;
    for (const auto& e : spec.events)
        if (e.type == TestEvent::Type::Clear) clears.push_back(e.frame);
    for (const auto& e : spec.events)
        if (e.type == TestEvent::Type::Dart)
            expected.push_back({e, roundOf(e.frame, clears), false});

    int failures = 0;
    for (auto& exp : expected) {
        RecordedHit* best = nullptr;
        for (auto& h : hits) {
            if (h.matched) continue;
            if (std::abs(h.frame - exp.ev.frame) > tolerance) continue;
            if (roundOf(h.frame, clears) != exp.round) continue;
            if (canonicalZone(h.zone) != canonicalZone(exp.ev.zone)) continue;
            if (!best ||
                std::abs(h.frame - exp.ev.frame) <
                    std::abs(best->frame - exp.ev.frame))
                best = &h;
        }
        if (best) {
            best->matched = true;
            exp.matched   = true;
            std::cout << "PASS  round " << (exp.round + 1)
                      << "  dart @" << exp.ev.frame
                      << "  " << exp.ev.zone
                      << "  (hit @" << best->frame
                      << " conf=" << best->confidence << ")\n";
        } else {
            ++failures;
            std::cout << "FAIL  round " << (exp.round + 1)
                      << "  dart @" << exp.ev.frame
                      << "  expected " << exp.ev.zone << " — ";
            // Nearest unmatched hit in the window, to show WHAT went wrong
            // (wrong zone) vs. nothing detected at all.  Consume it so the
            // same mistake isn't reported again as an extra hit.
            RecordedHit* near = nullptr;
            for (auto& h : hits) {
                if (h.matched) continue;
                if (std::abs(h.frame - exp.ev.frame) > tolerance) continue;
                if (roundOf(h.frame, clears) != exp.round) continue;
                if (!near ||
                    std::abs(h.frame - exp.ev.frame) <
                        std::abs(near->frame - exp.ev.frame))
                    near = &h;
            }
            if (near) {
                std::cout << "got " << near->zone << " @" << near->frame << '\n';
                near->matched = true;
            } else {
                std::cout << "no hit within ±" << tolerance << "f\n";
            }
        }
    }

    for (const auto& h : hits) {
        if (h.matched) continue;
        ++failures;
        std::cout << "FAIL  extra hit @" << h.frame
                  << "  " << h.zone << " (" << h.score << ")"
                  << "  conf=" << h.confidence << '\n';
    }

    const int total_checks = static_cast<int>(expected.size()) +
                             static_cast<int>(std::count_if(
                                 hits.begin(), hits.end(),
                                 [](const RecordedHit& h) { return !h.matched; }));
    std::cout << "[runtest] " << (total_checks - failures) << "/"
              << total_checks << " checks passed — "
              << (failures == 0 ? "PASS" : "FAIL") << '\n';
    return failures == 0 ? 0 : 1;
}
