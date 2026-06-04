#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

namespace camstream {

/// Writes decoded frames for each camera into separate MP4 files.
///
/// Files are named  cam0_YYYYMMDD_HHMMSS.mp4 … cam2_YYYYMMDD_HHMMSS.mp4
/// and are placed in @p output_dir (defaults to the current directory).
///
/// open() must be called once per camera, using the width/height/fps reported
/// by the server's Init packet.  writeFrame() is thread-safe and may be
/// called from the receive thread.  close() finalises all files.
class Recorder {
public:
    explicit Recorder(std::string output_dir = ".");
    ~Recorder();

    // Non-copyable, non-movable
    Recorder(const Recorder&)            = delete;
    Recorder& operator=(const Recorder&) = delete;

    /// Open the VideoWriter for one camera.  Call this when the Init packet
    /// for that camera arrives (so we know the real width/height/fps).
    /// @returns true on success, false if the file could not be created.
    bool open(int cam_id, int width, int height, int fps);

    /// Write one decoded BGR frame.  Thread-safe.
    void writeFrame(int cam_id, const cv::Mat& frame);

    /// Finalise and close all open video files.
    void close();

    bool isOpen(int cam_id) const;

private:
    /// Build a timestamped filename: <output_dir>/cam<id>_YYYYMMDD_HHMMSS.mp4
    std::string makeFilename(int cam_id) const;

    std::string output_dir_;

    struct Writer {
        cv::VideoWriter vw;
        std::mutex      mtx;   // protects vw – VideoWriter is not thread-safe
        bool            open{false};
    };

    // Fixed-size array; MAX_CAMERAS is 3 but we avoid the header dependency
    // by keeping a vector sized at construction time.
    std::vector<Writer*> writers_;
};

} // namespace camstream
