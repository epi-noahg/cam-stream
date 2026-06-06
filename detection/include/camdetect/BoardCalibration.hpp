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

    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);
};

} // namespace camdetect
