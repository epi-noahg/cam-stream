#include "DetectionService.hpp"

#include "camdetect/BoardCalibration.hpp"
#include "camdetect/Pipeline.hpp"
#include "camdetect/Types.hpp"
#include "camdetect/ZoneMap.hpp"

#include "CameraCapture.hpp"   // server/src — shared via include path

#include "sources/FileSource.hpp"

#include <chrono>
#include <cctype>
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

// Interleaved lockstep replay: read one frame from each source per tick and
// feed it with a shared timestamp (tick / fps) so multi-cam fusion groups
// votes by physical time, exactly like the live path.  Optionally paced at the
// video frame rate (so darts appear at a realistic cadence on the tablet) and
// optionally looped for continuous testing.
void DetectionService::feedLoopReplay_() {
    using clock = std::chrono::steady_clock;

    do {
        std::array<std::unique_ptr<camdetect::FileSource>, NUM_CAMS> srcs;
        double fps = cfg_.fps > 0 ? cfg_.fps : 30.0;
        bool ok = true;
        for (int c = 0; c < NUM_CAMS; ++c) {
            srcs[c] = std::make_unique<camdetect::FileSource>(cfg_.videoPaths[c]);
            if (!srcs[c]->isOpen()) {
                std::cerr << "[replay] cannot open " << cfg_.videoPaths[c] << "\n";
                ok = false;
                break;
            }
            if (srcs[c]->fps() > 0) fps = srcs[c]->fps();
            if (cfg_.offsets[c] > 0) srcs[c]->seek(cfg_.offsets[c]);
        }
        if (!ok) { running_ = false; return; }

        const auto wall_start = clock::now();
        cv::Mat frame;
        double ts_unused = 0.0;
        for (long tick = 0; running_; ++tick) {
            bool any = false;
            const double ts = tick / fps;
            for (int c = 0; c < NUM_CAMS; ++c) {
                if (srcs[c]->next(frame, ts_unused)) {
                    pipeline_->feedFrame(c, frame, ts);
                    any = true;
                }
            }
            if (!any) break;  // all sources exhausted

            if (cfg_.realtime) {  // pace to wall clock (no-op if we're behind)
                std::this_thread::sleep_until(
                    wall_start + std::chrono::duration_cast<clock::duration>(
                                     std::chrono::duration<double>(ts)));
            }
        }

        if (running_ && cfg_.loop) {
            std::cout << "[replay] loop\n";
            pipeline_->resetRound();
            pipeline_->refreshBackground();
        }
    } while (running_ && cfg_.loop);

    if (running_) finished_ = true;  // natural end (not a stop())
    std::cout << "[replay] finished\n";
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
