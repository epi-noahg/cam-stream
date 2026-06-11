#include "camdetect/Pipeline.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace camdetect {

namespace {

/// CAMDETECT_TRACE=1 dumps every per-cam hit, fusion emission and dedup
/// decision to stderr — the raw material for debugging a failed test run.
bool traceEnabled()
{
    static const bool on = [] {
        const char* v = std::getenv("CAMDETECT_TRACE");
        return v && v[0] && v[0] != '0';
    }();
    return on;
}

#define CAMTRACE(...) do { if (traceEnabled()) { \
    std::fprintf(stderr, __VA_ARGS__); std::fputc('\n', stderr); } } while (0)

int fusedVoteCount(const FusedHit& h)
{
    int n = 0;
    for (int i = 0; i < NUM_CAMS; ++i)
        if (h.per_cam[i].cam_id >= 0) ++n;
    return n;
}

// Same-dart association against committed round hits.
//
// A single physical dart can produce several fused-hit emissions when slow
// cameras stabilise in later fusion windows; with rim parallax and
// along-axis tip slide their board projections can disagree by tens of
// millimetres, so neither space nor time alone identifies "same dart".
//
// Two committed signals decide it:
//
//   * VOTER OVERLAP — a detector never re-emits a dart it already reported
//     (ghost regions are suppressed at the source), so a new fused hit that
//     shares a voting camera with a committed hit MUST be a new physical
//     dart, however close it landed.  This is what lets two darts sit 10 mm
//     apart in the same sector and still both score.
//
//   * Otherwise (disjoint voters) a new hit inside the space/time gates is a
//     late re-observation by cameras that hadn't reported that dart yet —
//     it is MERGED into the committed hit and the union re-fused, so a
//     well-sighted straggler can move a misread zone to the right one.
constexpr double ROUND_DEDUP_HARD_TIME_S =  1.0;
constexpr float  ROUND_DEDUP_MM          = 30.f;
constexpr double ROUND_DEDUP_TIME_S      =  1.8;
constexpr float  ROUND_DEDUP_TIME_MM     = 80.f;

bool votersOverlap(const FusedHit& a, const FusedHit& b)
{
    for (int i = 0; i < NUM_CAMS; ++i)
        if (a.per_cam[i].cam_id >= 0 && b.per_cam[i].cam_id >= 0)
            return true;
    return false;
}

} // anon

Pipeline::Pipeline(std::array<BoardCalibration, NUM_CAMS> calibrations,
                   double                                  fusion_window_seconds)
    : fusion_(fusion_window_seconds)
{
    for (int i = 0; i < NUM_CAMS; ++i)
        detectors_[i] = std::make_unique<DartDetector>(i, std::move(calibrations[i]));
}

void Pipeline::setOnHit(HitCallback cb)
{
    std::lock_guard<std::mutex> lk(mtx_);
    on_hit_ = std::move(cb);
}

void Pipeline::setOnHitUpdated(HitUpdateCallback cb)
{
    std::lock_guard<std::mutex> lk(mtx_);
    on_hit_updated_ = std::move(cb);
}

void Pipeline::setZoneMap(int cam_id, ZoneMap zm)
{
    if (cam_id < 0 || cam_id >= NUM_CAMS) return;
    std::lock_guard<std::mutex> lk(mtx_);
    detectors_[cam_id]->setZoneMap(std::move(zm));
}

void Pipeline::feedFrame(int cam_id, const cv::Mat& frame, double timestamp)
{
    if (cam_id < 0 || cam_id >= NUM_CAMS) return;

    std::optional<DartHit>  hit;
    std::optional<FusedHit> fused;
    HitCallback             cb;

    {
        std::lock_guard<std::mutex> lk(mtx_);

        // Advance the shared fusion clock with the newest timestamp seen on any
        // camera.  This is what lets an open fusion window close on time even
        // when one camera lags or never reports the current dart.
        if (timestamp > fusion_clock_) fusion_clock_ = timestamp;

        hit = detectors_[cam_id]->processFrame(frame, timestamp);

        // Cross-cam support: a dart that really sticks at hit->board_xy must
        // leave foreground in the peers' cumulative masks at the projected
        // pixel.  A shadow or reflection fools one camera but has no backing
        // anywhere else — fusion uses this to break junk-vs-real ties.
        if (hit) {
            for (int i = 0; i < NUM_CAMS; ++i) {
                if (i == cam_id) continue;
                const auto& det = *detectors_[i];
                if (!det.cumSupportValid() || !det.calib().isValid()) continue;
                ++hit->support_peers;
                const cv::Point2f px = det.calib().boardToImage(hit->board_xy);
                if (det.hasForegroundNear(px, 18.f)) ++hit->support_cams;
            }
        }

        if (hit)
            CAMTRACE("[trace] t=%.2f f=%d cam%d HIT zone=%s xy=(%.0f,%.0f) "
                     "conf=%.2f sigma=%.1f shape_q=%.2f margin=%.1f "
                     "axis=(%.2f,%.2f) s_along=%.0f s_across=%.1f sup=%d/%d",
                     timestamp, static_cast<int>(timestamp * 30 + 0.5), cam_id,
                     hit->zone.c_str(), hit->board_xy.x, hit->board_xy.y,
                     hit->confidence, hit->sigma_mm, hit->shape_q,
                     hit->zone_margin_mm, hit->axis_board.x, hit->axis_board.y,
                     hit->sigma_along_mm, hit->sigma_across_mm,
                     hit->support_cams, hit->support_peers);
        if (hit && darts_in_round_ >= MAX_DARTS_PER_ROUND)
            CAMTRACE("[trace] t=%.2f cam%d hit DROPPED (round full)",
                     timestamp, cam_id);

        if (darts_in_round_ < MAX_DARTS_PER_ROUND) {
            if (hit) fused = fusion_.addHit(*hit);          // all-cams fast path
            if (!fused) fused = fusion_.tick(fusion_clock_); // time-driven close
            if (fused) {
                if (traceEnabled()) {
                    int nv = fusedVoteCount(*fused);
                    CAMTRACE("[trace] t=%.2f f=%d FUSED zone=%s xy=(%.0f,%.0f) "
                             "conf=%.2f votes=%d",
                             fused->timestamp,
                             static_cast<int>(fused->timestamp * 30 + 0.5),
                             fused->zone.c_str(), fused->board_xy.x,
                             fused->board_xy.y, fused->confidence, nv);
                }
                bool is_dup           = false;
                bool updated          = false;
                const cv::Point2f cur = fused->board_xy;
                for (auto& prev : round_hits_) {
                    const double dt = std::abs(fused->timestamp - prev.timestamp);
                    const cv::Point2f p  = prev.board_xy;
                    const float       dx = cur.x - p.x;
                    const float       dy = cur.y - p.y;
                    const float       d2 = dx * dx + dy * dy;

                    const bool same_dart_gate =
                        dt < ROUND_DEDUP_HARD_TIME_S ||
                        d2 < ROUND_DEDUP_MM * ROUND_DEDUP_MM ||
                        (dt < ROUND_DEDUP_TIME_S &&
                         d2 < ROUND_DEDUP_TIME_MM * ROUND_DEDUP_TIME_MM);
                    if (!same_dart_gate) continue;

                    // Shared voter ⇒ that camera's detector saw NEW mask
                    // material (its ghosts are suppressed at the source), so
                    // this is a new physical dart landing close by — keep
                    // looking, and commit if no other hit claims it.
                    if (votersOverlap(prev, *fused)) {
                        CAMTRACE("[trace]   near %s (dt=%.2f d=%.0fmm) but "
                                 "voters overlap -> NEW dart",
                                 prev.zone.c_str(), dt, std::sqrt(d2));
                        continue;
                    }

                    // Disjoint voters: late re-observation of this committed
                    // dart.  Merge the vote sets and re-fuse — the union may
                    // both sharpen the position (axis crossing) and flip the
                    // zone to the better-sighted camera's label.
                    std::vector<DartHit> union_votes;
                    for (int i = 0; i < NUM_CAMS; ++i) {
                        if (prev.per_cam[i].cam_id >= 0)
                            union_votes.push_back(prev.per_cam[i]);
                        else if (fused->per_cam[i].cam_id >= 0)
                            union_votes.push_back(fused->per_cam[i]);
                    }
                    if (auto refused = MultiCamFusion::fuseVotes(union_votes)) {
                        refused->timestamp = prev.timestamp;
                        const bool changed = refused->zone != prev.zone;
                        CAMTRACE("[trace]   MERGE into %s -> %s (conf=%.2f, "
                                 "votes=%d)",
                                 prev.zone.c_str(), refused->zone.c_str(),
                                 refused->confidence,
                                 fusedVoteCount(*refused));
                        prev    = *refused;
                        updated = changed || true;
                        *fused  = *refused;   // callback reports the merged hit
                    }
                    is_dup = true;
                    break;
                }
                if (!is_dup) {
                    ++darts_in_round_;
                    round_hits_.push_back(*fused);
                    cb = on_hit_;
                    CAMTRACE("[trace]   COMMIT dart %d: %s",
                             darts_in_round_, fused->zone.c_str());
                } else if (updated) {
                    cb = on_hit_updated_;
                }
            }
        }
        // Watchdog counters are in master frames: tick them on one camera's
        // feed only, otherwise every threshold fires NUM_CAMS× too early —
        // bad enough to force a background warmup while the player is still
        // pulling darts, which poisons the reference for the whole next round.
        if (cam_id == 0) watchdogStuckHuman();
        maybeAutoReset();
    }

    if (cb && fused) cb(*fused);
}

void Pipeline::watchdogStuckHuman()
{
    // Cross-cam watchdog: if 2+ peers say OK and one is HumanBlob, the
    // outlier is almost certainly seeing lighting noise rather than a real
    // human → force a bg refresh on it after STUCK_HUMAN_FRAMES (~2s).
    int ok_count    = 0;
    int human_count = 0;
    for (int i = 0; i < NUM_CAMS; ++i) {
        const auto s = detectors_[i]->state();
        if (s == DetectorState::Normal || s == DetectorState::BoardClean)
            ++ok_count;
        if (s == DetectorState::HumanBlob)
            ++human_count;
    }

    for (int i = 0; i < NUM_CAMS; ++i) {
        const bool is_human = detectors_[i]->state() == DetectorState::HumanBlob;
        const bool peers_ok = ok_count >= 2;
        if (is_human && peers_ok) {
            if (++stuck_human_frames_[i] > STUCK_HUMAN_FRAMES) {
                detectors_[i]->refreshBackground();
                stuck_human_frames_[i] = 0;
            }
        } else {
            stuck_human_frames_[i] = 0;
        }
    }

    // Post-round backstop: when the round is complete (collect phase) and
    // ANY cam is still HUMAN, after POST_ROUND_STUCK_FRAMES (~5s) we force a
    // global bg refresh.  Covers the all-cams-stuck case (rim noise after
    // collect) that the per-cam watchdog can't break out of.
    const bool round_complete = (darts_in_round_ >= MAX_DARTS_PER_ROUND);
    if (round_complete && human_count > 0) {
        if (++post_round_human_frames_ > POST_ROUND_STUCK_FRAMES) {
            for (auto& d : detectors_) d->refreshBackground();
            post_round_human_frames_ = 0;
        }
    } else {
        post_round_human_frames_ = 0;
    }
}

RoundStatus Pipeline::roundStatus() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return computeRoundStatus_();
}

RoundStatus Pipeline::computeRoundStatus_() const
{
    bool any_human  = false;
    bool any_warmup = false;
    for (const auto& d : detectors_) {
        const auto s = d->state();
        if (s == DetectorState::HumanBlob) any_human  = true;
        if (s == DetectorState::Warmup)    any_warmup = true;
    }

    RoundStatus rs;
    if (any_warmup) {
        rs.phase   = RoundPhase::Resyncing;
        rs.message = "Initialising — please wait";
    } else if (any_human) {
        rs.phase   = RoundPhase::Resyncing;
        rs.message = "Wait — board is being cleaned";
    } else if (darts_in_round_ >= MAX_DARTS_PER_ROUND) {
        rs.phase   = RoundPhase::Complete;
        rs.message = "Collect your darts";
    } else {
        rs.phase     = RoundPhase::WaitingDart;
        rs.next_dart = darts_in_round_ + 1;
        rs.message   = "Throw dart " + std::to_string(rs.next_dart);
    }
    return rs;
}

void Pipeline::resetRound()
{
    std::lock_guard<std::mutex> lk(mtx_);
    fusion_.reset();
    for (auto& d : detectors_) d->reset();
    darts_in_round_ = 0;
    round_hits_.clear();
}

void Pipeline::setDiffThreshold(float v)
{
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& d : detectors_) d->setDiffThreshold(v);
}

float Pipeline::diffThreshold() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return detectors_[0] ? detectors_[0]->diffThreshold() : 0.f;
}

void Pipeline::setLineMergePerpPx(float v)
{
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& d : detectors_) d->setLineMergePerpPx(v);
}

float Pipeline::lineMergePerpPx() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return detectors_[0] ? detectors_[0]->lineMergePerpPx() : 0.f;
}

std::vector<FusedHit> Pipeline::roundHits() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return round_hits_;
}

void Pipeline::refreshBackground(int cam_id)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (cam_id < 0) {
        for (auto& d : detectors_) d->refreshBackground();
    } else if (cam_id < NUM_CAMS) {
        detectors_[cam_id]->refreshBackground();
    }
}

DetectorViz Pipeline::camViz(int cam_id) const
{
    if (cam_id < 0 || cam_id >= NUM_CAMS) return {};
    std::lock_guard<std::mutex> lk(mtx_);
    return detectors_[cam_id]->lastViz();
}

int Pipeline::dartsInRound() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return darts_in_round_;
}

void Pipeline::maybeAutoReset()
{
    if (darts_in_round_ == 0) return;

    if (traceEnabled()) {
        static double last_log = -1e9;
        if (fusion_clock_ - last_log >= 1.0) {
            last_log = fusion_clock_;
            char buf[256];
            int  off = std::snprintf(buf, sizeof(buf),
                                     "[trace] t=%.2f reset-watch darts=%d",
                                     fusion_clock_, darts_in_round_);
            for (int i = 0; i < NUM_CAMS; ++i) {
                const auto viz = detectors_[i]->lastViz();
                off += std::snprintf(buf + off, sizeof(buf) - off,
                                     "  cam%d{st=%d fg=%d cln=%d}",
                                     i, static_cast<int>(viz.state),
                                     viz.fg_px_cumulative, viz.clean_frames);
            }
            CAMTRACE("%s", buf);
        }
    }

    for (const auto& d : detectors_)
        if (!d->boardLooksCleared()) return;

    fusion_.reset();
    for (auto& d : detectors_) d->reset();
    darts_in_round_ = 0;
    round_hits_.clear();
}

} // namespace camdetect
