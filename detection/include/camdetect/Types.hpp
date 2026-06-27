#pragma once

#include <array>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace camdetect {

constexpr int NUM_CAMS = 3;

/// Standard ISO/BDO dartboard geometry, in millimetres from the bullseye.
namespace board {
    constexpr float BULLSEYE_RADIUS =   6.35f;  // 50 pts
    constexpr float BULL_RADIUS     =  16.0f;   // 25 pts
    constexpr float TRIPLE_INNER    =  99.0f;
    constexpr float TRIPLE_OUTER    = 107.0f;
    constexpr float DOUBLE_INNER    = 162.0f;
    constexpr float DOUBLE_OUTER    = 170.0f;

    /// Sector values clockwise starting at 12 o'clock ("20" on top).
    constexpr std::array<int, 20> SECTORS = {
        20, 1, 18, 4, 13, 6, 10, 15, 2, 17,
        3, 19, 7, 16, 8, 11, 14, 9, 12, 5
    };
} // namespace board

enum class DetectorState : uint8_t {
    Warmup,      // building initial background
    Normal,      // ready / tracking
    HumanBlob,   // big blob in frame (hand/arm), detection paused
    BoardClean   // mask empty for a while, ready for next round
};

/// Snapshot of a single detector's internal state, for the debug UI.
struct DetectorViz {
    std::vector<cv::Point>   contour;
    cv::Point2f              tip_pixel    {};
    cv::Point2f              tail_pixel   {};
    cv::Point2f              dart_dir     {};
    cv::Point2f              board_xy     {};
    bool                     has_detection{false};
    double                   timestamp    {0.0};
    cv::Mat                  mask;
    /// Binary mask of all pixels considered part of the dart after
    /// fragment-merging along the refined axis (CV_8U, 0/255).  Lets the
    /// debug UI tint the full "claimed" dart region — useful when the dart
    /// is split into pieces by a dark sector and we want to verify which
    /// fragments we glued back together.
    cv::Mat                  dart_region;
    std::vector<cv::Point2f> logged_tips_px;
    DetectorState            state        {DetectorState::Warmup};
    /// Diagnostics for the round-reset logic: foreground pixel count against
    /// the empty-board reference, and how many consecutive frames it has
    /// been below the clean threshold.
    int                      fg_px_cumulative {0};
    int                      clean_frames     {0};
};

/// A dart hit reported by one camera, in board coordinates (mm).
///
/// Beyond the position and label, each hit carries an honest estimate of how
/// trustworthy this camera's observation is, so fusion can weight cameras by
/// what they actually see instead of treating them as equals:
///
///   sigma_mm   1-sigma positional uncertainty of board_xy.  Derived from the
///              local homography scale at the tip pixel (a grazing-angle cam
///              maps 1 px of tip error to many mm of board error), the tip
///              jitter observed during the stability window, and the shape
///              quality.  Small sigma == this camera has a good view of THIS
///              dart at THIS spot.
///   zone_margin_mm  distance from the tip to the nearest scoring boundary
///              (wire / ring edge).  A dart in the middle of single-20 keeps
///              its label even with 5 mm of error; one grazing a wire doesn't.
///   shape_q    silhouette quality [0..1]: elongation + tip sharpness.  Low
///              when the mask is blob-like, fragmented or seen end-on.
///   view_q     viewing-geometry quality [0..1] at the tip (informational;
///              already folded into sigma_mm).
///   confidence P(zone label is correct) ~= erf(margin / (sigma*sqrt(2)))
///              attenuated by shape_q.  THE per-cam score used for voting.
struct DartHit {
    int         cam_id    {-1};
    cv::Point2f board_xy  {};        // mm, origin = bullseye, +y = sector 20
    cv::Point2f tip_pixel {};        // tip in image coordinates
    std::string zone      {};        // "T20", "D5", "20", "Bull", "25", "MISS"
    int         score     {0};
    float       sigma_mm        {10.f};
    float       zone_margin_mm  {0.f};
    float       shape_q         {0.f};
    float       view_q          {0.f};
    float       confidence      {0.f};
    double      timestamp {0.0};

    /// Anisotropic error model of board_xy, for fusion.  Tip mislocalisation
    /// is dominated by mask truncation ALONG the dart's image axis (the part
    /// of the dart crossing dark sectors drops out of the mask, so the tip
    /// estimate slides up the shaft) while the axis fit nails the
    /// PERPENDICULAR direction to a couple of pixels.  Propagated to board
    /// space, each camera therefore constrains the tip tightly across
    /// `axis_board` and loosely along it; crossing several cameras'
    /// constraints recovers the precise point even when each individual
    /// estimate slid along its own axis.
    cv::Point2f axis_board      {0.f, 0.f};  // unit vector, tip→tail
    float       sigma_along_mm  {25.f};
    float       sigma_across_mm {3.f};

    /// Cross-camera support: a dart physically present at board_xy must show
    /// up as foreground in the OTHER cams' cumulative masks at the projected
    /// pixel; a shadow / reflection / ghost detection has no such backing.
    /// Filled by Pipeline (the detector can't see its peers).
    int         support_cams  {0};   // peers with foreground at the spot
    int         support_peers {0};   // peers whose mask was usable
};

/// The single hit obtained by fusing votes from all cameras.
struct FusedHit {
    std::string                           zone;
    int                                   score      {0};
    float                                 confidence {0.f};
    cv::Point2f                           board_xy   {};   // fused position, mm
    float                                 sigma_mm   {10.f}; // fused 1-sigma, mm
    /// Fused 1-sigma uncertainty along the RADIAL direction at board_xy (mm).
    /// The ring decision (single / triple / double — 8 mm bands) is only as
    /// trustworthy as this: when no camera has a near-tangential view of the
    /// spot, radius lives in the loose along-axis direction and sigma_r_mm is
    /// large, so the ring call must degrade to the safe single.
    float                                 sigma_r_mm {10.f};
    std::array<DartHit, NUM_CAMS>         per_cam    {};
    double                                timestamp  {0.0};
};

} // namespace camdetect
