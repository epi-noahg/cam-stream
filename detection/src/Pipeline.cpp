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

int fusedVoteCount(const FusedHit& h)
{
    int n = 0;
    for (int i = 0; i < NUM_CAMS; ++i)
        if (h.per_cam[i].cam_id >= 0) ++n;
    return n;
}

// Same-dart dedup against committed round hits.
//
// A single physical dart can produce up to NUM_CAMS independent fused-hit
// emissions when the cams stabilise far enough apart in time to fall into
// different fusion windows.  Each emission is single-cam, and with rim
// parallax their board projections can disagree by tens of millimetres.  So
// space-only dedup is not enough.
//
// The strongest signal we have is TIME: nobody throws two real darts in less
// than ~1 second.  We therefore drop any new fused hit that lands inside
// ROUND_DEDUP_HARD_TIME_S of an existing one, regardless of position.  Above
// that, we still apply the looser combined space/time fallback so that
// stragglers a bit past the hard window get caught when the position is
// roughly consistent.
constexpr double ROUND_DEDUP_HARD_TIME_S =  1.0;
constexpr float  ROUND_DEDUP_MM          = 30.f;
constexpr double ROUND_DEDUP_TIME_S      =  1.8;
constexpr float  ROUND_DEDUP_TIME_MM     = 80.f;

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
                // Dedup with REPLACE-IF-BETTER: when the same physical dart
                // produces multiple FusedHits in close succession, we keep
                // the one backed by more cameras (or, tied on votes, by
                // higher per-cam confidence).  This lets late stragglers
                // CORRECT an early misread instead of being silently
                // dropped — addresses the "wrong cam wins because it
                // emitted first" pathology.
                bool is_dup            = false;
                const cv::Point2f cur  = fusedCentroid(*fused);
                const int new_votes    = fusedVoteCount(*fused);
                const float new_conf   = fused->confidence;
                for (auto& prev : round_hits_) {
                    const double dt = std::abs(fused->timestamp - prev.timestamp);

                    auto maybeReplace = [&]() {
                        const int prev_votes = fusedVoteCount(prev);
                        const bool better = (new_votes > prev_votes) ||
                            (new_votes == prev_votes && new_conf > prev.confidence);
                        if (better) prev = *fused;
                    };

                    // Hard time gate: same physical dart for sure.
                    if (dt < ROUND_DEDUP_HARD_TIME_S) {
                        maybeReplace();
                        is_dup = true;
                        break;
                    }

                    const cv::Point2f p  = fusedCentroid(prev);
                    const float       dx = cur.x - p.x;
                    const float       dy = cur.y - p.y;
                    const float       d2 = dx * dx + dy * dy;
                    if (d2 < ROUND_DEDUP_MM * ROUND_DEDUP_MM) {
                        maybeReplace();
                        is_dup = true;
                        break;
                    }
                    if (dt < ROUND_DEDUP_TIME_S &&
                        d2 < ROUND_DEDUP_TIME_MM * ROUND_DEDUP_TIME_MM) {
                        maybeReplace();
                        is_dup = true;
                        break;
                    }
                }
                if (!is_dup) {
                    ++darts_in_round_;
                    round_hits_.push_back(*fused);
                    cb = on_hit_;
                }
            }
        }
        watchdogStuckHuman();
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
    for (const auto& d : detectors_)
        if (!d->boardLooksCleared()) return;

    fusion_.reset();
    for (auto& d : detectors_) d->reset();
    darts_in_round_ = 0;
    round_hits_.clear();
}

} // namespace camdetect
