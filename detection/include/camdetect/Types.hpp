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
    std::vector<cv::Point2f> logged_tips_px;
    DetectorState            state        {DetectorState::Warmup};
};

/// A dart hit reported by one camera, in board coordinates (mm).
struct DartHit {
    int         cam_id    {-1};
    cv::Point2f board_xy  {};        // mm, origin = bullseye, +y = sector 20
    std::string zone      {};        // "T20", "D5", "20", "Bull", "25", "MISS"
    int         score     {0};
    float       confidence{0.f};
    double      timestamp {0.0};
};

/// The single hit obtained by fusing votes from all cameras.
struct FusedHit {
    std::string                           zone;
    int                                   score      {0};
    float                                 confidence {0.f};
    std::array<DartHit, NUM_CAMS>         per_cam    {};
    double                                timestamp  {0.0};
};

} // namespace camdetect
