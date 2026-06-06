#pragma once

#include <opencv2/core.hpp>
#include <string>

namespace camdetect {

/// Abstract source of timed BGR frames.  Concrete impl: FileSource (cv::VideoCapture).
/// Live mode does not use a FrameSource — frames are pushed directly via Pipeline::feedFrame().
class FrameSource {
public:
    virtual ~FrameSource() = default;

    /// Reads the next frame.  @returns false on EOF or error.
    /// @p timestamp is in seconds (file: PTS from VideoCapture; live: arbitrary clock).
    virtual bool next(cv::Mat& frame, double& timestamp) = 0;

    virtual int  width()  const = 0;
    virtual int  height() const = 0;
    virtual int  fps()    const = 0;
};

} // namespace camdetect
