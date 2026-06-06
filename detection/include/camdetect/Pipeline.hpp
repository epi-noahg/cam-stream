#pragma once

#include "BoardCalibration.hpp"
#include "DartDetector.hpp"
#include "MultiCamFusion.hpp"
#include "Types.hpp"

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace camdetect {

/// High-level phase of the current throwing round, surfaced for player-facing
/// UI.  Computed from `darts_in_round_` and the per-cam detector states.
enum class RoundPhase {
    WaitingDart,   ///< throw the next dart
    Complete,      ///< 3 darts done, go collect them
    Resyncing,     ///< human in frame / background re-learning
};

struct RoundStatus {
    RoundPhase  phase     {RoundPhase::WaitingDart};
    int         next_dart {1};   ///< 1..3, meaningful only when WaitingDart
    std::string message;         ///< short label for the UI
};

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

    // 0.4s window (~12 frames @ 30fps): wide enough to absorb the per-cam
    // stability lag — different views of the same dart stabilise at different
    // master moments, so we need to wait for the slowest cam to converge.
    // Darts are thrown >1s apart, so this won't merge two physical darts.
    explicit Pipeline(std::array<BoardCalibration, NUM_CAMS> calibrations,
                      double fusion_window_seconds = 0.4);

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

    /// High-level round status (phase + human-readable message).  Computed
    /// from current detector states + dart count, no extra state machine.
    RoundStatus roundStatus() const;

private:
    void maybeAutoReset();              ///< clear round if all cams are quiet
    void watchdogStuckHuman();          ///< force-resync a lone-HUMAN cam
    RoundStatus computeRoundStatus_() const;  ///< lock-held helper

    mutable std::mutex                                  mtx_;
    std::array<std::unique_ptr<DartDetector>, NUM_CAMS> detectors_;
    MultiCamFusion                                      fusion_;
    HitCallback                                         on_hit_;
    int                                                 darts_in_round_{0};
    double                                              fusion_clock_{-1.0};
    std::vector<FusedHit>                               round_hits_;
    std::array<int, NUM_CAMS>                           stuck_human_frames_{};
    int                                                 post_round_human_frames_{0};

    static constexpr int                                MAX_DARTS_PER_ROUND  = 3;
    /// ~2s @ 30fps: how long one cam can be HUMAN while ≥2 peers are OK
    /// before we decide it's a noise hallucination and force-resync it.
    static constexpr int                                STUCK_HUMAN_FRAMES   = 60;
    /// ~5s @ 30fps: when the round is complete and any cam is still HUMAN,
    /// after this long we force a global bg refresh.  Backstop for the case
    /// where ALL cams hallucinate human together (rim noise after a collect).
    static constexpr int                                POST_ROUND_STUCK_FRAMES = 150;
};

} // namespace camdetect
