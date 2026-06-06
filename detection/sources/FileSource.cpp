#include "FileSource.hpp"

namespace camdetect {

FileSource::FileSource(const std::string& path)
    : cap_(path)
{
    if (!cap_.isOpened()) return;
    width_         = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    height_        = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
    fps_           = static_cast<int>(cap_.get(cv::CAP_PROP_FPS));
    total_frames_  = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_COUNT));
    if (fps_ <= 0) fps_ = 30;
}

FileSource::~FileSource() = default;

bool FileSource::next(cv::Mat& frame, double& timestamp)
{
    if (!cap_.read(frame) || frame.empty()) return false;
    timestamp = cap_.get(cv::CAP_PROP_POS_MSEC) / 1000.0;
    // POS_FRAMES is the index of the NEXT frame after read(); the one we just
    // got is therefore POS_FRAMES - 1.  Keep current_frame_ = "last shown".
    current_frame_ = static_cast<int>(cap_.get(cv::CAP_PROP_POS_FRAMES)) - 1;
    return true;
}

bool FileSource::seek(int frame_idx)
{
    if (!cap_.isOpened()) return false;
    if (frame_idx < 0) frame_idx = 0;
    if (total_frames_ > 0 && frame_idx >= total_frames_)
        frame_idx = total_frames_ - 1;
    if (!cap_.set(cv::CAP_PROP_POS_FRAMES, frame_idx)) return false;
    current_frame_ = frame_idx;
    return true;
}

} // namespace camdetect
