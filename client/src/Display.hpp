#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

namespace camstream {

/// Thread-safe multi-camera viewer.
///
/// updateFrame() may be called from any thread.
/// render() MUST be called from the main thread (OpenCV / Cocoa requirement).
class Display {
public:
    /// @param num_cameras  Number of video feeds to show side-by-side.
    /// @param cam_width    Width of each individual feed in pixels.
    /// @param cam_height   Height of each individual feed in pixels.
    Display(int num_cameras, int cam_width, int cam_height);
    ~Display();

    // Non-copyable, non-movable
    Display(const Display&)            = delete;
    Display& operator=(const Display&) = delete;

    /// Replace the stored frame for camera @p cam_id.  Thread-safe.
    void updateFrame(int cam_id, cv::Mat frame);

    /// Composite all feeds into one window and call cv::imshow.
    /// @returns false when the user closes the window or presses ESC.
    bool render();

private:
    int num_cameras_;
    int cam_width_;
    int cam_height_;

    // One frame buffer + one mutex per camera
    std::vector<cv::Mat>  frames_;
    std::vector<std::mutex*> mutexes_;   // raw pointers because std::mutex is non-movable

    const std::string window_name_{"CamStream – 3 cameras"};
};

} // namespace camstream
