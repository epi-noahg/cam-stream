#pragma once

#include "BoardCalibration.hpp"
#include "DartDetector.hpp"
#include "MultiCamFusion.hpp"
#include "Types.hpp"

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace camdetect {

/// Orchestrates per-camera detection and multi-camera fusion.
///
/// Thread-safety: feedFrame() may be called concurrently from any number of
/// threads, including one per camera.  setOnHit() must be called before the
/// first feedFrame().
///
/// Round semantics: at most 3 hits are emitted per round; further emissions
/// are suppressed until all detectors report the board looks cleared, at
/// which point the round auto-resets.
class Pipeline {
public:
    using HitCallback = std::function<void(const FusedHit&)>;

    // 0.5s window absorbs the measured inter-camera detection lag (~0.4s): a
    // camera that stabilises on the same dart a few frames later still lands in
    // the same fusion window.  Darts are thrown >1s apart, so this won't merge
    // two physical darts.
    explicit Pipeline(std::array<BoardCalibration, NUM_CAMS> calibrations,
                      double fusion_window_seconds = 0.5);

    void setOnHit(HitCallback cb);

    /// Push one decoded BGR frame from camera @p cam_id (0..NUM_CAMS-1).
    void feedFrame(int cam_id, const cv::Mat& frame, double timestamp);

    /// Force a round reset (normally automatic on board-clear).
    void resetRound();

    /// Drop and re-learn the background for one or all cameras.
    void refreshBackground(int cam_id = -1);

    /// Live-tunable detection threshold on the LAB diff distance.
    void  setDiffThreshold(float v);
    float diffThreshold() const;

    /// Live-tunable perp distance for collinear-fragment merging (px).
    void  setLineMergePerpPx(float v);
    float lineMergePerpPx() const;

    /// Latest debug snapshot for one camera.
    DetectorViz camViz(int cam_id) const;

    /// All confirmed FusedHits in the current round (max 3, in throw order).
    std::vector<FusedHit> roundHits() const;

    int dartsInRound() const;

private:
    void maybeAutoReset();   // checks all detectors → clear round if all quiet

    mutable std::mutex                                  mtx_;
    std::array<std::unique_ptr<DartDetector>, NUM_CAMS> detectors_;
    MultiCamFusion                                      fusion_;
    HitCallback                                         on_hit_;
    int                                                 darts_in_round_{0};
    double                                              fusion_clock_{-1.0};
    std::vector<FusedHit>                               round_hits_;
    static constexpr int                                MAX_DARTS_PER_ROUND = 3;
};

} // namespace camdetect
