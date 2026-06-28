// camdetect_stress
// ─────────────────────────────────────────────────────────────────────────────
// Concurrency stress harness for Pipeline::feedFrame.
//
// camdetect_runtest replays a session in lockstep on ONE thread, so it never
// exercises the per-detector locking that lets the three cameras' detection
// pipelines run in parallel (Pipeline docs feedFrame as concurrent; the live
// DetectionService runs one worker thread per camera).  This tool reproduces
// that topology: it drives each camera from its own thread as fast as it
// decodes, while a "poker" thread hammers the read-side query methods
// (camViz / roundStatus / diffThreshold / …) concurrently.  Between passes it
// fires the operator-command paths (refreshBackground / resetRound).
//
// Purpose is RACE DETECTION, not zone correctness (free-running threads don't
// group fusion windows deterministically).  Run it under a ThreadSanitizer
// build to prove the locking is data-race-free:
//
//   cmake -S detection -B detection/build-tsan -DCMAKE_BUILD_TYPE=Debug \
//         -DCMAKE_CXX_FLAGS=-fsanitize=thread \
//         -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread
//   cmake --build detection/build-tsan -j --target camdetect_stress
//   detection/build-tsan/camdetect_stress detection/tests/session1.yml
//
// Exit 0 = ran to completion.  TSan reports any race to stderr (and, with
// halt_on_error=1, aborts non-zero).
//
// Usage: camdetect_stress test.yml [--frames N] [--passes P]

#include "../sources/FileSource.hpp"
#include "TestSpec.hpp"
#include "camdetect/BoardCalibration.hpp"
#include "camdetect/Pipeline.hpp"
#include "camdetect/Types.hpp"
#include "camdetect/ZoneMap.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace camdetect;

int main(int argc, char* argv[])
{
    std::vector<std::string> args(argv + 1, argv + argc);

    int max_frames = 1200;
    int passes     = 1;
    for (size_t i = 0; i < args.size();) {
        if (args[i] == "--frames" && i + 1 < args.size()) {
            max_frames = std::stoi(args[i + 1]);
            args.erase(args.begin() + i, args.begin() + i + 2);
        } else if (args[i] == "--passes" && i + 1 < args.size()) {
            passes = std::stoi(args[i + 1]);
            args.erase(args.begin() + i, args.begin() + i + 2);
        } else {
            ++i;
        }
    }
    if (args.size() != 1) {
        std::cerr << "Usage: " << argv[0]
                  << " test.yml [--frames N] [--passes P]\n";
        return 1;
    }

    TestSpec spec;
    if (!spec.load(args[0])) {
        std::cerr << "Failed to load test spec: " << args[0] << '\n';
        return 1;
    }

    std::array<BoardCalibration, NUM_CAMS> calibs;
    for (int i = 0; i < NUM_CAMS; ++i) {
        if (!calibs[i].loadFromFile(spec.calibs[i])) {
            std::cerr << "Failed to load calibration: " << spec.calibs[i] << '\n';
            return 1;
        }
    }

    Pipeline pipeline(calibs);
    for (int i = 0; i < NUM_CAMS; ++i) {
        ZoneMap zm;
        if (zm.loadFromFile(ZoneMap::companionPath(spec.calibs[i])))
            pipeline.setZoneMap(i, std::move(zm));
    }

    std::atomic<int> hit_count{0};
    std::atomic<int> upd_count{0};
    pipeline.setOnHit([&](const FusedHit&) { hit_count.fetch_add(1); });
    pipeline.setOnHitUpdated([&](const FusedHit&) { upd_count.fetch_add(1); });

    const int fps = spec.fps > 0 ? spec.fps : 30;

    std::cout << "[stress] " << args[0] << "  threads=" << NUM_CAMS
              << "  frames<=" << max_frames << "  passes=" << passes << '\n';

    for (int pass = 0; pass < passes; ++pass) {
        std::atomic<bool>        feeding{true};
        std::vector<std::thread> threads;

        // One feed thread per camera — the live capture/processing topology.
        for (int c = 0; c < NUM_CAMS; ++c) {
            threads.emplace_back([&, c] {
                FileSource src(spec.videos[c]);
                if (!src.isOpen()) {
                    std::cerr << "Failed to open video: " << spec.videos[c]
                              << '\n';
                    return;
                }
                if (spec.offsets[c] > 0) src.seek(spec.offsets[c]);
                cv::Mat frame;
                double  src_ts = 0.0;
                for (int f = 0; f < max_frames; ++f) {
                    if (!src.next(frame, src_ts)) break;
                    pipeline.feedFrame(c, frame, double(f) / double(fps));
                }
            });
        }

        // Poker thread: read-side queries concurrent with detection.
        std::thread poker([&] {
            while (feeding.load(std::memory_order_relaxed)) {
                for (int i = 0; i < NUM_CAMS; ++i) pipeline.camViz(i);
                pipeline.roundStatus();
                pipeline.diffThreshold();
                pipeline.lineMergePerpPx();
                pipeline.dartsInRound();
                pipeline.roundHits();
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        });

        for (auto& t : threads) t.join();
        feeding.store(false, std::memory_order_relaxed);
        poker.join();

        // Operator-command paths touch the same locks; fire them between passes.
        pipeline.refreshBackground();
        pipeline.resetRound();
    }

    std::cout << "[stress] done — hits=" << hit_count.load()
              << " updates=" << upd_count.load() << '\n';
    return 0;
}
