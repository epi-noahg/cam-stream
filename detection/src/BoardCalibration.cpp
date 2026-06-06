#include "camdetect/BoardCalibration.hpp"

#include <opencv2/calib3d.hpp>
#include <opencv2/core/persistence.hpp>

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
