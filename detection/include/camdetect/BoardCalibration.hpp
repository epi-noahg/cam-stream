#pragma once

#include <opencv2/core.hpp>
#include <string>

namespace camdetect {

/// Per-camera calibration relative to one dartboard.
///
/// Stores a homography that maps image points (pixels) to board points
/// (millimetres, origin = bullseye, +y axis pointing toward sector 20).
struct BoardCalibration {
    cv::Mat     homography_img_to_board;  // 3x3 CV_64F
    cv::Mat     homography_board_to_img;  // cached inverse
    cv::Point2f bullseye_pixel  {};
    float       orientation_deg {0.f};    // rotation of sector 20 from image up
    int         image_width     {0};
    int         image_height    {0};
    float       diff_threshold  {-1.f};   // per-cam LAB-diff threshold; <0 = use detector default

    bool isValid() const { return !homography_img_to_board.empty(); }

    cv::Point2f imageToBoard(const cv::Point2f& p) const;
    cv::Point2f boardToImage(const cv::Point2f& p) const;

    /// Local image→board scale at pixel @p px: the {min, max} singular values
    /// of the 2×2 Jacobian of the homography mapping, in mm per pixel
    /// (computed by central finite differences).
    ///
    /// `max` answers "how many mm does board_xy move for 1 px of tip error in
    /// the worst direction" — large for cameras seeing the board at a grazing
    /// angle, so it directly quantifies viewing-geometry quality at that spot.
    /// `min` is the best-direction scale, useful as a conservative px→mm
    /// conversion for distances.  Returns {0,0} when not calibrated.
    cv::Vec2f localScaleMmPerPx(const cv::Point2f& px) const;

    /// Full 2×2 Jacobian d(board_mm)/d(pixel) at @p px, by central finite
    /// differences.  Maps a small pixel-space displacement to its board-space
    /// displacement — the directional version of localScaleMmPerPx, needed to
    /// propagate an anisotropic tip error (large along the dart's image axis,
    /// small across it) into board coordinates.  Zero matrix when invalid.
    cv::Matx22f localJacobian(const cv::Point2f& px) const;

    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);
};

} // namespace camdetect
