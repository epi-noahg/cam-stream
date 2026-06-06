#include "camdetect/Pipeline.hpp"

namespace camdetect {

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
                ++darts_in_round_;
                round_hits_.push_back(*fused);
                cb = on_hit_;
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
