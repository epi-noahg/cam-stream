#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

#include <opencv2/opencv.hpp>

namespace camstream {

/// A single captured frame from one webcam.
struct RawFrame {
    int     cam_id;        ///< Camera index [0, MAX_CAMERAS)
    cv::Mat bgr;           ///< Full BGR image
    int64_t timestamp_ms;  ///< Monotonic time since capture started, in ms
};

using FrameCallback = std::function<void(RawFrame)>;

/// Opens a V4L2 device and continuously delivers frames via a callback.
/// The callback is invoked from the internal capture thread.
class CameraCapture {
public:
    /// @param cam_id   Logical camera index used to tag RawFrame.
    /// @param device   V4L2 path, e.g. "/dev/video0".
    /// @param width    Requested capture width (best-effort).
    /// @param height   Requested capture height (best-effort).
    /// @param fps      Requested frame rate (best-effort).
    CameraCapture(int cam_id, const std::string& device,
                  int width, int height, int fps);
    ~CameraCapture();

    // Non-copyable, non-movable
    CameraCapture(const CameraCapture&)            = delete;
    CameraCapture& operator=(const CameraCapture&) = delete;

    /// Starts the capture thread; callback receives frames until stop().
    void start(FrameCallback callback);

    /// Signals the capture thread to stop and waits for it to finish.
    void stop();

    bool isRunning() const noexcept { return running_.load(); }

private:
    void loop();

    int           cam_id_;
    std::string   device_;
    int           width_, height_, fps_;
    FrameCallback callback_;
    std::thread   thread_;
    std::atomic<bool> running_{false};
};

} // namespace camstream
