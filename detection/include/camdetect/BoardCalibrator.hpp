#pragma once

#include "BoardCalibration.hpp"

#include <opencv2/core.hpp>
#include <vector>

namespace camdetect {

/// Builds a BoardCalibration from a frame, either automatically or from
/// user-supplied reference points (clic 4 points).
class BoardCalibrator {
public:
    /// Auto-detection: Hough circles for the rings + colour masks (red/green
    /// doubles/triples) to lock orientation.  Fills @p out, returns true on success.
    bool detectAuto(const cv::Mat& frame, BoardCalibration& out) const;

    /// Manual calibration from four reference points on the outer DOUBLE wire:
    ///   points[0] = sector 20 (top,    12 o'clock)
    ///   points[1] = sector  6 (right,   3 o'clock)
    ///   points[2] = sector  3 (bottom,  6 o'clock)
    ///   points[3] = sector 11 (left,    9 o'clock)
    /// Bullseye location is computed from the homography.
    bool fromReferencePoints(const std::vector<cv::Point2f>& points,
                             int                              image_width,
                             int                              image_height,
                             BoardCalibration&                out) const;
};

} // namespace camdetect
