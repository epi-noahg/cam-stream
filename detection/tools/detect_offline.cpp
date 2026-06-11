// camdetect_offline
// ─────────────────────────────────────────────────────────────────────────────
// Replays three pre-recorded videos through the Pipeline and prints every
// FusedHit to stdout.  Useful for iterating on the algorithm against the
// recordings in client/.
//
// Usage:
//   camdetect_offline cam0.mp4 cam1.mp4 cam2.mp4 cam0.yml cam1.yml cam2.yml

#include "camdetect/BoardCalibration.hpp"
#include "camdetect/Pipeline.hpp"
#include "camdetect/Types.hpp"
#include "camdetect/ZoneMap.hpp"
#include "../sources/FileSource.hpp"

#include <array>
#include <iostream>
#include <memory>

using namespace camdetect;

int main(int argc, char* argv[])
{
    if (argc < 1 + 2 * NUM_CAMS) {
        std::cerr << "Usage: " << argv[0]
                  << " cam0.mp4 cam1.mp4 cam2.mp4"
                  << " cam0.yml cam1.yml cam2.yml\n";
        return 1;
    }

    // ── Load calibrations ────────────────────────────────────────────────────
    std::array<BoardCalibration, NUM_CAMS> calibs;
    for (int i = 0; i < NUM_CAMS; ++i) {
        if (!calibs[i].loadFromFile(argv[1 + NUM_CAMS + i])) {
            std::cerr << "Failed to load calibration: "
                      << argv[1 + NUM_CAMS + i] << '\n';
            return 1;
        }
    }

    // ── Open sources ─────────────────────────────────────────────────────────
    std::array<std::unique_ptr<FileSource>, NUM_CAMS> sources;
    for (int i = 0; i < NUM_CAMS; ++i) {
        sources[i] = std::make_unique<FileSource>(argv[1 + i]);
        if (!sources[i]->isOpen()) {
            std::cerr << "Failed to open video: " << argv[1 + i] << '\n';
            return 1;
        }
    }

    // ── Run pipeline ─────────────────────────────────────────────────────────
    Pipeline pipeline(std::move(calibs));

    // Pixel-accurate zone maps (camN_zones.png next to camN.yml), if present.
    for (int i = 0; i < NUM_CAMS; ++i) {
        ZoneMap zm;
        const std::string zpath = ZoneMap::companionPath(argv[1 + NUM_CAMS + i]);
        if (zm.loadFromFile(zpath)) {
            pipeline.setZoneMap(i, std::move(zm));
            std::cout << "[cam" << i << "] pixel zone map: " << zpath << '\n';
        }
    }
    pipeline.setOnHit([](const FusedHit& h) {
        std::cout << "[hit] t=" << h.timestamp
                  << " zone=" << h.zone
                  << " score=" << h.score
                  << " conf=" << h.confidence
                  << " sigma=" << h.sigma_mm << "mm\n";
        for (const auto& c : h.per_cam) {
            if (c.cam_id < 0) continue;
            std::cout << "      cam" << c.cam_id
                      << " zone=" << c.zone
                      << " conf=" << c.confidence
                      << " sigma=" << c.sigma_mm << "mm"
                      << " margin=" << c.zone_margin_mm << "mm"
                      << " shape=" << c.shape_q
                      << " view=" << c.view_q
                      << " tip=(" << c.tip_pixel.x << ',' << c.tip_pixel.y << ')'
                      << " mm=(" << c.board_xy.x << ',' << c.board_xy.y << ")\n";
        }
    });

    // Simple round-robin replay; deeper sync (timestamps from PTS) once needed.
    bool any_running = true;
    while (any_running) {
        any_running = false;
        for (int i = 0; i < NUM_CAMS; ++i) {
            cv::Mat frame;
            double  ts = 0.0;
            if (sources[i]->next(frame, ts)) {
                pipeline.feedFrame(i, frame, ts);
                any_running = true;
            }
        }
    }
    return 0;
}
