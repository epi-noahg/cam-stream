#include "DetectionService.hpp"

#include "camdetect/BoardCalibration.hpp"
#include "camdetect/Pipeline.hpp"
#include "camdetect/Types.hpp"
#include "camdetect/ZoneMap.hpp"

#include "CameraCapture.hpp"   // server/src — shared via include path

#include "sources/FileSource.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <iostream>

namespace dart::detect {

using dart::game::Throw;

// ── Zone label → Throw ───────────────────────────────────────────────────────
Throw zoneToThrow(const std::string& zone) {
    if (zone.empty() || zone == "MISS") return Throw{0, 1, false};
    if (zone == "Bull" || zone == "BULL" || zone == "50") return Throw{50, 1, false};
    if (zone == "25") return Throw{25, 1, false};

    int mult = 1;
    std::size_t pos = 0;
    if (zone[0] == 'T' || zone[0] == 't') { mult = 3; pos = 1; }
    else if (zone[0] == 'D' || zone[0] == 'd') { mult = 2; pos = 1; }

    int value = 0;
    for (; pos < zone.size(); ++pos) {
        if (!std::isdigit(static_cast<unsigned char>(zone[pos]))) break;
        value = value * 10 + (zone[pos] - '0');
    }
    if (value == 25 || value == 50) mult = 1;  // bull never multiplied
    return Throw{value, mult, false};
}

// ── Live capture handles (kept out of the header) ───────────────────────────
struct DetectionService::LiveCams {
    std::array<std::unique_ptr<camstream::CameraCapture>, NUM_CAMS> caps;
};

namespace {
CamReady mapState(camdetect::DetectorState s) {
    switch (s) {
        case camdetect::DetectorState::Warmup:    return CamReady::Warmup;
        case camdetect::DetectorState::Normal:    return CamReady::Normal;
        case camdetect::DetectorState::HumanBlob: return CamReady::Human;
        case camdetect::DetectorState::BoardClean:return CamReady::Clean;
    }
    return CamReady::Warmup;
}

const char* phaseStr(camdetect::RoundPhase p) {
    switch (p) {
        case camdetect::RoundPhase::WaitingDart: return "waiting";
        case camdetect::RoundPhase::Complete:    return "complete";
        case camdetect::RoundPhase::Resyncing:   return "resyncing";
    }
    return "waiting";
}
} // namespace

DetectionService::DetectionService(dart::game::GameManager& gm) : gm_(gm) {}
DetectionService::~DetectionService() { stop(); }

bool DetectionService::init(const Config& cfg) {
    cfg_ = cfg;

    std::array<camdetect::BoardCalibration, camdetect::NUM_CAMS> calibs;
    for (int c = 0; c < NUM_CAMS; ++c) {
        if (!calibs[c].loadFromFile(cfg_.calibPaths[c])) {
            std::cerr << "[detection] failed to load calibration: "
                      << cfg_.calibPaths[c] << "\n";
            return false;
        }
    }

    pipeline_ = std::make_unique<camdetect::Pipeline>(calibs);

    // Attach pixel-accurate zone maps when present (camN_zones.png).
    for (int c = 0; c < NUM_CAMS; ++c) {
        camdetect::ZoneMap zm;
        const std::string zpath =
            camdetect::ZoneMap::companionPath(cfg_.calibPaths[c]);
        if (zm.loadFromFile(zpath)) {
            pipeline_->setZoneMap(c, std::move(zm));
            std::cout << "[detection] cam" << c << " zone map: " << zpath << "\n";
        }
    }

    pipeline_->setOnHit([this](const camdetect::FusedHit& h) {
        Throw thr = zoneToThrow(h.zone);
        dart::game::ThrowMeta meta;
        meta.detected    = true;
        meta.confidence  = h.confidence;
        meta.needsReview = h.confidence < cfg_.reviewThreshold;
        meta.zone        = h.zone;
        gm_.recordDetectedThrow(thr, meta);
    });

    return true;
}

void DetectionService::start() {
    if (!pipeline_ || running_) return;
    running_ = true;

    if (cfg_.replay) {
        feed_threads_.emplace_back([this] { feedLoopReplay_(); });
    } else {
        live_ = std::make_unique<LiveCams>();
        for (int c = 0; c < NUM_CAMS; ++c)
            feed_threads_.emplace_back([this, c] { feedLoopCamera_(c); });
    }
    status_thread_ = std::thread([this] { statusLoop_(); });
}

void DetectionService::stop() {
    running_ = false;
    if (live_)
        for (auto& cap : live_->caps) if (cap) cap->stop();
    for (auto& t : feed_threads_) if (t.joinable()) t.join();
    feed_threads_.clear();
    if (status_thread_.joinable()) status_thread_.join();
}

// Interleaved lockstep replay with transport controls (pause / step / seek)
// driven by the optional control window on the Mac.
//
//   • Frames are fed to the pipeline with a MONOTONIC timestamp (a feed
//     counter / fps) so multi-cam fusion groups votes by physical time even
//     after a backward seek.
//   • `replay_pos_` tracks the display position (can jump on seek).
//   • A seek refreshes the background + resets the round so scrubbing never
//     injects a spurious dart into the game.
//   • Playback is paced at the video fps (skipped while stepping).
void DetectionService::feedLoopReplay_() {
    using clock = std::chrono::steady_clock;

    std::array<std::unique_ptr<camdetect::FileSource>, NUM_CAMS> srcs;
    double fps = cfg_.fps > 0 ? cfg_.fps : 30.0;
    int total = INT32_MAX;
    for (int c = 0; c < NUM_CAMS; ++c) {
        srcs[c] = std::make_unique<camdetect::FileSource>(cfg_.videoPaths[c]);
        if (!srcs[c]->isOpen()) {
            std::cerr << "[replay] cannot open " << cfg_.videoPaths[c] << "\n";
            running_ = false;
            return;
        }
        if (srcs[c]->fps() > 0) fps = srcs[c]->fps();
        total = std::min(total, srcs[c]->totalFrames() - cfg_.offsets[c]);
    }
    replay_total_ = std::max(0, total);

    auto seekAll = [&](int tick) {
        for (int c = 0; c < NUM_CAMS; ++c) srcs[c]->seek(cfg_.offsets[c] + tick);
    };
    seekAll(0);

    int  pos        = 0;     // display position
    long feed_count = 0;     // monotonic, for fusion timestamps
    auto wall_start = clock::now();
    long played     = 0;     // frames since last pacing reset

    cv::Mat frame;
    double ts_unused = 0.0;

    while (running_) {
        // ── Seek request ────────────────────────────────────────────────
        const int target = seek_target_.exchange(-1);
        if (target >= 0) {
            pos = std::max(0, target);
            seekAll(pos);
            pipeline_->refreshBackground();
            pipeline_->resetRound();
            wall_start = clock::now();
            played = 0;
        }

        // ── Pause (unless a step was requested) ─────────────────────────
        bool doStep = false;
        if (step_.load() > 0) { step_.fetch_sub(1); doStep = true; }
        if (paused_.load() && !doStep) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        // ── Feed one tick ───────────────────────────────────────────────
        const double ts = feed_count / fps;
        std::array<cv::Mat, NUM_CAMS> snap;
        bool any = false;
        for (int c = 0; c < NUM_CAMS; ++c) {
            if (srcs[c]->next(frame, ts_unused)) {
                pipeline_->feedFrame(c, frame, ts);
                snap[c] = frame.clone();
                any = true;
            }
        }

        if (!any) {  // end of video
            if (cfg_.loop) {
                seekAll(0); pos = 0;
                pipeline_->refreshBackground();
                pipeline_->resetRound();
                wall_start = clock::now(); played = 0;
                continue;
            }
            paused_ = true;            // hold on last frame, allow scrub-back
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        {
            std::lock_guard<std::mutex> lk(display_mtx_);
            display_frames_ = snap;
        }
        ++feed_count;
        ++pos;
        replay_pos_ = pos;

        if (cfg_.realtime && !doStep) {  // pace to wall clock (playing only)
            ++played;
            std::this_thread::sleep_until(
                wall_start + std::chrono::duration_cast<clock::duration>(
                                 std::chrono::duration<double>(played / fps)));
        }
    }

    if (running_) finished_ = true;
    std::cout << "[replay] finished\n";
}

// ── Replay transport controls ───────────────────────────────────────────────
void DetectionService::replayTogglePause() { paused_ = !paused_.load(); }

void DetectionService::replayStep(int frames) {
    paused_ = true;
    step_.fetch_add(std::max(1, frames));
}

void DetectionService::replaySeek(int frameTick) {
    seek_target_ = std::max(0, frameTick);
}

bool DetectionService::replaySnapshot(cv::Mat& out, int maxWidth) const {
    std::array<cv::Mat, NUM_CAMS> frames;
    {
        std::lock_guard<std::mutex> lk(display_mtx_);
        for (int c = 0; c < NUM_CAMS; ++c)
            if (!display_frames_[c].empty()) frames[c] = display_frames_[c];
    }
    // Stack available cams horizontally at a common height.
    int h = 0;
    for (const auto& f : frames) if (!f.empty()) h = std::max(h, f.rows);
    if (h == 0) return false;

    std::vector<cv::Mat> tiles;
    for (const auto& f : frames) {
        if (f.empty()) continue;
        cv::Mat t;
        if (f.rows != h) {
            const double s = static_cast<double>(h) / f.rows;
            cv::resize(f, t, cv::Size(static_cast<int>(f.cols * s), h));
        } else {
            t = f;
        }
        tiles.push_back(t);
    }
    cv::hconcat(tiles, out);
    if (maxWidth > 0 && out.cols > maxWidth) {
        const double s = static_cast<double>(maxWidth) / out.cols;
        cv::resize(out, out, cv::Size(maxWidth, static_cast<int>(out.rows * s)));
    }
    return true;
}

void DetectionService::feedLoopCamera_(int cam_id) {
    auto cap = std::make_unique<camstream::CameraCapture>(
        cam_id, cfg_.devices[cam_id], cfg_.width, cfg_.height, cfg_.fps);

    const auto session_start = std::chrono::steady_clock::now();
    cap->start([this, session_start](camstream::RawFrame rf) {
        const double ts = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - session_start).count();
        pipeline_->feedFrame(rf.cam_id, rf.bgr, ts);
    });
    live_->caps[cam_id] = std::move(cap);
}

void DetectionService::statusLoop_() {
    while (running_) {
        BoardStatus s = computeStatus_();
        bool changed;
        {
            std::lock_guard<std::mutex> lk(status_mtx_);
            changed = (s != last_status_);
            last_status_ = s;
        }
        if (changed && on_status_) on_status_(s);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

BoardStatus DetectionService::computeStatus_() const {
    BoardStatus s;
    bool allReady = true;
    for (int c = 0; c < NUM_CAMS; ++c) {
        const CamReady st = mapState(pipeline_->camViz(c).state);
        const bool ready = (st == CamReady::Normal || st == CamReady::Clean);
        s.cams[c] = CamStatus{c, st, ready};
        if (!ready) allReady = false;
    }
    const camdetect::RoundStatus rs = pipeline_->roundStatus();
    s.round.phase    = phaseStr(rs.phase);
    s.round.nextDart = rs.next_dart;
    s.round.message  = rs.message;
    s.allReady       = allReady;
    return s;
}

BoardStatus DetectionService::boardStatus() const {
    std::lock_guard<std::mutex> lk(status_mtx_);
    return last_status_;
}

void DetectionService::resetRound() {
    if (pipeline_) pipeline_->resetRound();
}

void DetectionService::refreshBackground() {
    if (pipeline_) pipeline_->refreshBackground();
}

} // namespace dart::detect
