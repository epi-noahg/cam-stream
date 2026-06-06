#pragma once

#include "camdetect/FrameSource.hpp"

#include <opencv2/videoio.hpp>
#include <string>

namespace camdetect {

/// Reads frames sequentially from a video file via cv::VideoCapture.
class FileSource final : public FrameSource {
public:
    explicit FileSource(const std::string& path);
    ~FileSource() override;

    bool next(cv::Mat& frame, double& timestamp) override;

    int width()  const override { return width_;  }
    int height() const override { return height_; }
    int fps()    const override { return fps_;    }

    bool isOpen()       const { return cap_.isOpened(); }
    int  totalFrames()  const { return total_frames_; }
    int  currentFrame() const { return current_frame_; }

    /// Seek to an absolute frame index (clamped to [0, total_frames-1]).
    bool seek(int frame_idx);

private:
    cv::VideoCapture cap_;
    int              width_         {0};
    int              height_        {0};
    int              fps_           {0};
    int              total_frames_  {0};
    int              current_frame_ {0};
};

} // namespace camdetect
