#include "camdetect/BoardCalibrator.hpp"

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>

namespace camdetect {

bool BoardCalibrator::detectAuto(const cv::Mat& /*frame*/,
                                 BoardCalibration& /*out*/) const
{
    // TODO: Hough circles on grayscale + red/green colour masks to find the
    // bullseye and lock orientation, then build the homography.
    return false;
}

bool BoardCalibrator::fromReferencePoints(const std::vector<cv::Point2f>& points,
                                          int  image_width,
                                          int  image_height,
                                          BoardCalibration& out) const
{
    if (points.size() != 4) return false;

    // Reference points in board space (mm).  All four on the outer double
    // wire at cardinal sectors — gives a non-degenerate set (no 3 collinear).
    //   0: sector 20 (top)    (0,  R)
    //   1: sector 6  (right)  (R,  0)
    //   2: sector 3  (bottom) (0, -R)
    //   3: sector 11 (left)   (-R, 0)
    constexpr float R = 170.f;   // DOUBLE_OUTER
    const std::vector<cv::Point2f> board_pts = {
        {  0.f,  R   },
        {  R,    0.f },
        {  0.f, -R   },
        { -R,    0.f }
    };

    cv::Mat H = cv::findHomography(points, board_pts);
    if (H.empty()) return false;

    out.homography_img_to_board = H;
    out.homography_board_to_img = H.inv();
    out.orientation_deg         = 0.f;   // points already encode rotation
    out.image_width             = image_width;
    out.image_height            = image_height;

    // Bullseye is computed by reprojecting (0,0) board → image
    std::vector<cv::Point2f> bull_in{ {0.f, 0.f} }, bull_out;
    cv::perspectiveTransform(bull_in, bull_out, out.homography_board_to_img);
    out.bullseye_pixel = bull_out.front();
    return true;
}

} // namespace camdetect
