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
    /// Fired when late camera votes refine an ALREADY-emitted hit (position
    /// and possibly zone).  The hit keeps its original timestamp, so
    /// consumers can locate the entry to amend.
    using HitUpdateCallback = std::function<void(const FusedHit&)>;

    // 1.0s window (~30 frames @ 30fps): wide enough that even a slow cam
    // (whose mask noise needs the full STABLE_FRAMES_REQUIRED to settle
    // ~half a second later than the fastest cam) still falls in the same
    // fusion window as its peers.  Without this, late stragglers emit in a
    // NEW window as single-cam hits and steal the score from the legitimate
    // 2-cam cluster.  Darts are thrown >1s apart, so this won't merge two
    // physical darts.
    explicit Pipeline(std::array<BoardCalibration, NUM_CAMS> calibrations,
                      double fusion_window_seconds = 1.0);

    void setOnHit(HitCallback cb);
    void setOnHitUpdated(HitUpdateCallback cb);

    /// Attach a pixel-accurate zone map (AutoCalibrator output) to one
    /// camera's detector.  Call before the first feedFrame() ideally; safe
    /// at any time.
    void setZoneMap(int cam_id, ZoneMap zm);

    /// Replace one camera's board calibration at runtime.  Recreates that
    /// camera's detector (its background re-learns over the next frames), so
    /// any zone map must be re-attached afterwards via setZoneMap().  Safe to
    /// call concurrently with feedFrame().  Used by the calibration UI to
    /// apply a fresh AutoCalibrator result without restarting the server.
    void setCalibration(int cam_id, BoardCalibration calib);

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

    /// Re-determine a fused hit's zone from the TRIANGULATED crossing instead
    /// of trusting each camera's own (noisy) tip label.  Votes among the
    /// geometric lookup at the crossing and each camera's pixel-accurate zone
    /// map sampled at the crossing projected back into that camera, weighted
    /// by how clearly each read sits inside its zone (boundary margin) and by
    /// the camera's viewing quality at that spot.  Also recomputes a calibrated
    /// confidence that honestly collapses for single-camera (un-triangulated)
    /// hits, whose tip can have slid along the dart axis.  Lock-held helper.
    void refineFusedZone_(FusedHit& f) const;

    /// EPIPOLAR RECALL.  Just after a dart is committed with fewer than
    /// NUM_CAMS voters, ask each silent peer to look for the same dart at the
    /// committed point projected into it (boardToImage), and — if a thin,
    /// persistent, slide-consistent component is there — merge that vote in and
    /// re-fuse.  Turns fragile votes=1 hits into votes≥2 (tighter position,
    /// higher confidence) with near-zero false-positive cost, because the
    /// search is geometry-anchored and gated by fuseVotes' slide consistency.
    /// Lock-held; mutates @p h (a round_hits_ entry) in place.
    void recallSilentPeers_(FusedHit& h);
    /// Search radius (px) around the projected contact point for recall: large
    /// enough to span a single camera's along-slide projected into the peer.
    static constexpr float RECALL_RADIUS_PX = 50.f;

    mutable std::mutex                                  mtx_;
    std::array<std::unique_ptr<DartDetector>, NUM_CAMS> detectors_;
    MultiCamFusion                                      fusion_;
    HitCallback                                         on_hit_;
    HitUpdateCallback                                   on_hit_updated_;
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
