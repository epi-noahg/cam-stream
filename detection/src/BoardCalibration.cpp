#include "camdetect/BoardCalibration.hpp"

#include <cmath>
#include <opencv2/calib3d.hpp>
#include <opencv2/core/persistence.hpp>
#include <vector>

namespace camdetect {

cv::Point2f BoardCalibration::imageToBoard(const cv::Point2f& p) const
{
    if (homography_img_to_board.empty()) return {};
    std::vector<cv::Point2f> in{p}, out;
    cv::perspectiveTransform(in, out, homography_img_to_board);
    return out.front();
}

cv::Point2f BoardCalibration::boardToImage(const cv::Point2f& p) const
{
    if (homography_board_to_img.empty()) return {};
    std::vector<cv::Point2f> in{p}, out;
    cv::perspectiveTransform(in, out, homography_board_to_img);
    return out.front();
}

cv::Vec2f BoardCalibration::localScaleMmPerPx(const cv::Point2f& px) const
{
    if (homography_img_to_board.empty()) return {0.f, 0.f};

    // Central differences with a 2 px step — large enough to be numerically
    // clean on a CV_64F homography, small enough to stay local.
    constexpr float H = 2.f;
    std::vector<cv::Point2f> in{
        {px.x + H, px.y}, {px.x - H, px.y},
        {px.x, px.y + H}, {px.x, px.y - H}};
    std::vector<cv::Point2f> out;
    cv::perspectiveTransform(in, out, homography_img_to_board);

    // Jacobian columns: d(board)/dx and d(board)/dy, in mm per px.
    const float a = (out[0].x - out[1].x) / (2.f * H);   // dXb/dxi
    const float c = (out[0].y - out[1].y) / (2.f * H);   // dYb/dxi
    const float b = (out[2].x - out[3].x) / (2.f * H);   // dXb/dyi
    const float d = (out[2].y - out[3].y) / (2.f * H);   // dYb/dyi

    // Singular values of [[a,b],[c,d]] via the eigenvalues of JᵀJ.
    const float tr  = a*a + b*b + c*c + d*d;
    const float det = a*d - b*c;
    const float disc = std::sqrt(std::max(0.f, tr*tr - 4.f*det*det));
    const float s_max = std::sqrt(std::max(0.f, (tr + disc) * 0.5f));
    const float s_min = std::sqrt(std::max(0.f, (tr - disc) * 0.5f));
    return {s_min, s_max};
}

bool BoardCalibration::saveToFile(const std::string& path) const
{
    cv::FileStorage fs(path, cv::FileStorage::WRITE);
    if (!fs.isOpened()) return false;
    fs << "homography_img_to_board" << homography_img_to_board
       << "homography_board_to_img" << homography_board_to_img
       << "bullseye_px_x"           << bullseye_pixel.x
       << "bullseye_px_y"           << bullseye_pixel.y
       << "orientation_deg"         << orientation_deg
       << "image_width"             << image_width
       << "image_height"            << image_height
       << "diff_threshold"          << diff_threshold;
    return true;
}

bool BoardCalibration::loadFromFile(const std::string& path)
{
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) return false;
    fs["homography_img_to_board"] >> homography_img_to_board;
    fs["homography_board_to_img"] >> homography_board_to_img;
    fs["bullseye_px_x"]           >> bullseye_pixel.x;
    fs["bullseye_px_y"]           >> bullseye_pixel.y;
    fs["orientation_deg"]         >> orientation_deg;
    fs["image_width"]             >> image_width;
    fs["image_height"]            >> image_height;
    if (!fs["diff_threshold"].empty()) fs["diff_threshold"] >> diff_threshold;
    return isValid();
}

} // namespace camdetect
