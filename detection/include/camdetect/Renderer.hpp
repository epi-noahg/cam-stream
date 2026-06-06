#pragma once

#include "BoardCalibration.hpp"
#include "Types.hpp"

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace camdetect {

/// Top-down canonical drawing of a dartboard and helpers to overlay the
/// reprojected board onto camera frames.  Everything here is pure rendering;
/// no detection logic.
class Renderer {
public:
    /// Returns a square BGR image showing the standard dartboard layout,
    /// @p size pixels per side.  Origin (board 0,0) is at the image centre.
    static cv::Mat renderCanonicalBoard(int size = 500);

    /// Draw a hit (board-space mm) onto a canonical board image produced by
    /// renderCanonicalBoard().  @p label may be empty.
    static void drawHitOnCanonical(cv::Mat&         canonical,
                                    const cv::Point2f& board_xy,
                                    const cv::Scalar&  color,
                                    const std::string& label = "");

    /// Project the canonical board rings and sector wires onto a camera
    /// frame using @p calib.  Useful to verify a calibration visually.
    static void drawCalibrationOverlay(cv::Mat&                 frame,
                                        const BoardCalibration&  calib);

    /// Draw the detected dart blob contour + PCA axis + tip pixel.
    /// Any of these may be empty / default-valued to skip drawing.
    static void drawDetectionOverlay(cv::Mat&                       frame,
                                      const std::vector<cv::Point>&  contour,
                                      const cv::Point2f&             tip_pixel,
                                      const cv::Point2f&             dart_dir);

    /// Draw the dart as a line tip→tail with markers at each end.
    static void drawFullDart(cv::Mat&           frame,
                              const cv::Point2f& tip_pixel,
                              const cv::Point2f& tail_pixel);

    /// Faint marker for an already-logged dart (so the user remembers it's tracked).
    static void drawLoggedTip(cv::Mat&           frame,
                               const cv::Point2f& tip,
                               const cv::Scalar&  color);
};

} // namespace camdetect
