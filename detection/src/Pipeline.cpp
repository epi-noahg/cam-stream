#include "camdetect/Pipeline.hpp"

#include <cmath>

namespace camdetect {

namespace {

// Centroid of the per-cam projections inside a FusedHit (skips empty slots).
cv::Point2f fusedCentroid(const FusedHit& h)
{
    cv::Point2f sum{0.f, 0.f};
    int n = 0;
    for (int i = 0; i < NUM_CAMS; ++i) {
        if (h.per_cam[i].cam_id < 0) continue;
        sum.x += h.per_cam[i].board_xy.x;
        sum.y += h.per_cam[i].board_xy.y;
        ++n;
    }
    if (n == 0) return {0.f, 0.f};
    return {sum.x / n, sum.y / n};
}

// Two FusedHits refer to the same physical dart when:
//   - their projections are tight in space (<= ROUND_DEDUP_MM), OR
//   - they arrived close in time AND within a looser radius (rim parallax
//     can split per-cam projections by 30-50 mm even though it's one dart).
// Darts can't physically be thrown closer than ~0.8 s, so the time check is
// safe against legitimate distinct hits.
constexpr float  ROUND_DEDUP_MM      = 30.f;
constexpr double ROUND_DEDUP_TIME_S  =  0.8;
constexpr float  ROUND_DEDUP_TIME_MM = 70.f;

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

        if (darts_in_round_ < MAX_DARTS_PER_ROUND) {
            if (hit) fused = fusion_.addHit(*hit);          // all-cams fast path
            if (!fused) fused = fusion_.tick(fusion_clock_); // time-driven close
            if (fused) {
                // Dedup: when fusion windows split (single-cam stragglers
                // come in late and start their own window) the same physical
                // dart can produce two FusedHits.  Suppress the second one
                // if it lands close in space, or close in time + loosely in
                // space (rim parallax can split a single dart by 30-50 mm).
                bool is_dup = false;
                const cv::Point2f cur = fusedCentroid(*fused);
                for (const auto& prev : round_hits_) {
                    const cv::Point2f p  = fusedCentroid(prev);
                    const float       dx = cur.x - p.x;
                    const float       dy = cur.y - p.y;
                    const float       d2 = dx * dx + dy * dy;
                    if (d2 < ROUND_DEDUP_MM * ROUND_DEDUP_MM) {
                        is_dup = true; break;
                    }
                    const double dt = std::abs(fused->timestamp - prev.timestamp);
                    if (dt < ROUND_DEDUP_TIME_S &&
                        d2 < ROUND_DEDUP_TIME_MM * ROUND_DEDUP_TIME_MM) {
                        is_dup = true; break;
                    }
                }
                if (!is_dup) {
                    ++darts_in_round_;
                    round_hits_.push_back(*fused);
                    cb = on_hit_;
                }
            }
        }
        maybeAutoReset();
    }

    if (cb && fused) cb(*fused);
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
    for (const auto& d : detectors_)
        if (!d->boardLooksCleared()) return;

    fusion_.reset();
    for (auto& d : detectors_) d->reset();
    darts_in_round_ = 0;
    round_hits_.clear();
}

} // namespace camdetect
