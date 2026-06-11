#pragma once

#include "BoardCalibration.hpp"
#include "Types.hpp"
#include "ZoneMap.hpp"

#include <opencv2/core.hpp>
#include <optional>
#include <vector>

namespace camdetect {

/// Single-camera dart detection using LAB reference-frame differencing.
///
/// First N frames build a median background image; subsequent frames are
/// thresholded against it.  A foreground blob with dart-like aspect ratio,
/// stable for STABLE_FRAMES_REQUIRED frames, is reported as a DartHit.
class DartDetector {
public:
    DartDetector(int cam_id, BoardCalibration calib);

    /// @returns a confirmed DartHit, or std::nullopt while waiting.
    std::optional<DartHit> processFrame(const cv::Mat& frame, double timestamp);

    /// True after a long stretch of frames without any foreground blob — used
    /// by Pipeline to auto-reset the round when the player has cleared the board.
    bool boardLooksCleared() const;

    /// Drop the current background and re-learn from the next frames.
    void refreshBackground();

    /// Reset transient detection state (called between darts / rounds).  Keeps
    /// the learned background.
    void reset();

    /// Snapshot of internal state for the debug UI.  Thread-unsafe wrt
    /// processFrame; call from the same thread.
    DetectorViz lastViz() const { return last_viz_; }

    /// Current detector state — used by Pipeline for cross-cam coordination
    /// (e.g. force-resync a cam stuck in HumanBlob when its peers say OK).
    DetectorState state() const { return last_viz_.state; }

    /// Live-tunable detection threshold on the LAB diff distance (5..80).
    void  setDiffThreshold(float v) { diff_threshold_ = v; }
    float diffThreshold() const     { return diff_threshold_; }

    /// Perp distance (px) for merging collinear mask fragments into one dart.
    void  setLineMergePerpPx(float v) { line_merge_perp_px_ = v; }
    float lineMergePerpPx() const     { return line_merge_perp_px_; }

    /// Pixel-accurate zone map (from AutoCalibrator).  When set, scoring
    /// reads the zone at the tip PIXEL instead of projecting through the
    /// homography — exact even with lens distortion.
    void setZoneMap(ZoneMap zm) { zone_map_ = std::move(zm); }

    int cam_id() const { return cam_id_; }

    const BoardCalibration& calib() const { return calib_; }

    /// True when the cumulative (empty-board reference) mask is currently
    /// usable as a witness for cross-cam support checks: background learned
    /// and no human occluding the board.
    bool cumSupportValid() const;

    /// Does the latest cumulative mask contain real foreground within
    /// @p radius_px of @p px?  Used by Pipeline to test whether THIS camera
    /// can see a dart that a peer claims to have found.
    bool hasForegroundNear(const cv::Point2f& px, float radius_px) const;

private:
    int                               cam_id_;
    BoardCalibration                  calib_;
    ZoneMap                           zone_map_;

    cv::Mat                           bg_lab_;        // "empty-board" ref (CV_32FC3, LAB)
    cv::Mat                           throw_bg_lab_;  // snapped after each commit; empty before first throw
    cv::Mat                           bg_acc_;        // accumulator during warmup
    int                               warmup_count_   {0};
    bool                              warmup_done_    {false};
    cv::Mat                           roi_mask_;      // CV_8U, board region only

    int                               stable_frames_  {0};
    int                               candidate_gap_  {0};
    int                               quiet_frames_   {0};
    float                             jitter_sq_sum_  {0.f};  // Σ movement² over the stable window
    int                               jitter_n_       {0};
    cv::Point2f                       last_tip_pixel_ {};
    bool                              has_candidate_  {false};
    bool                              emitted_        {false};   // suppress dupes

    DetectorViz                       last_viz_       {};
    float                             diff_threshold_     {15.f};
    float                             line_merge_perp_px_ { 6.f};
    std::vector<cv::Point2f>          logged_tips_px_;
    /// Union of the (dilated) dart regions of every dart committed this
    /// round — the per-dart "individual masks".  A new candidate whose own
    /// region mostly overlaps this union is a ghost of an already-committed
    /// dart (it settled / lighting shifted), not a new throw.
    cv::Mat                           committed_regions_;
    /// Latest cumulative mask (diff vs the empty-board reference) and
    /// whether it is human-free — for cross-cam support queries.
    cv::Mat                           latest_cum_mask_;
    bool                              cum_mask_clean_ {false};
    int                               clean_frames_       {0};
    bool                              human_seen_         {false};
    int                               consecutive_small_  {0};
    int                               artifact_frames_    {0};
    cv::Point2f                       prev_big_centroid_  {};
    bool                              has_prev_big_centroid_ {false};
    int                               static_big_frames_  {0};

    static constexpr int   WARMUP_FRAMES_REQUIRED       = 30;
    // Higher → wait longer for the dart to settle before emitting; lower
    // means we may emit while the dart is still wobbling on impact or even
    // in motion.  15 frames ≈ 0.5s @30fps is a good middle ground.
    static constexpr int   STABLE_FRAMES_REQUIRED       = 15;
    /// A marginal-contrast dart flickers out of the mask for a frame or two
    /// (threshold beat against sensor noise).  Without a grace window every
    /// dropout restarts the stability count and a hard-to-see dart takes
    /// 3-4× longer to confirm — long enough for the NEXT throw to land and
    /// steal the window.
    static constexpr int   CANDIDATE_GAP_GRACE          = 2;
    static constexpr int   CLEAN_FRAMES_FOR_RESET       = 45;
    static constexpr int   CLEAN_FG_PIXELS              = 800;
    static constexpr int   POST_HUMAN_QUIET_FG_PIXELS   = 1500;   // mask "really" empty
    static constexpr int   POST_HUMAN_QUIET_FRAMES      = 8;      // ~0.27s settle
    static constexpr float HUGE_CONTOUR_AREA            = 15000.f;
    static constexpr float BG_UPDATE_ALPHA              = 0.015f;
    static constexpr float CHROMA_DIFF_THRESH           =     6.f;  // LAB a*b* dist
    static constexpr int   ARTIFACT_SNAP_FRAMES         = 30;       // ~1s residual
    static constexpr float HUMAN_MIN_SOLIDITY           =     0.55f;
    /// Per-frame centroid motion (px) below which the biggest blob counts as
    /// static.  Real humans always jitter; lighting drift is pixel-stable.
    /// Bumped from 3 to 6 because contour-boundary noise on big blobs can
    /// shift the centroid 3-5 px between frames without anyone moving.
    static constexpr float STATIC_BLOB_MOTION_PX        =     6.f;
    /// Consecutive frames of below-threshold motion before classifying a
    /// large blob as static noise (≈0.83s @ 30fps).
    static constexpr int   STATIC_BLOB_FRAMES_REQ       =    25;
    /// Area (px) of the smallest coherent blob we still treat as "human is
    /// present".  Below this, post-human exit is allowed even if there's
    /// plenty of speckle noise elsewhere — that noise is residual bg drift,
    /// not a person.
    static constexpr float HUMAN_PRESENT_AREA           =  3000.f;
    /// Fraction of a candidate's region overlapping committed-dart regions
    /// above which it's classified as a ghost re-detection.  A genuinely new
    /// dart that merely crosses an old one overlaps ~20-30%; a settling
    /// ghost overlaps most of its area.
    static constexpr float GHOST_OVERLAP_FRAC           =     0.55f;
    static constexpr int   COMMITTED_REGION_DILATE_PX   =     7;
    static constexpr float TIP_BACKING_MIN_SOLIDITY     =     0.40f;
    static constexpr float TIP_BACKING_MIN_AREA         =   100.f;
    static constexpr float TIP_BACKING_SEARCH_PX        =    20.f;
    static constexpr int   MAX_LOGGED_TIPS              =    12;  // tip-suppression memory, not round limit
};

} // namespace camdetect
