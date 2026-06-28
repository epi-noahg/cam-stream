#include "CameraCapture.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace camstream {

CameraCapture::CameraCapture(int cam_id, const std::string& device,
                             int width, int height, int fps)
    : cam_id_(cam_id), device_(device)
    , width_(width), height_(height), fps_(fps)
{}

CameraCapture::~CameraCapture()
{
    stop();
}

void CameraCapture::start(FrameCallback callback)
{
    callback_ = std::move(callback);
    running_  = true;
    thread_   = std::thread(&CameraCapture::loop, this);
}

void CameraCapture::stop()
{
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void CameraCapture::loop()
{
    cv::VideoCapture cap(device_, cv::CAP_V4L2);
    if (!cap.isOpened()) {
        std::cerr << "[CameraCapture] cam" << cam_id_
                  << ": cannot open " << device_ << "\n";
        running_ = false;
        return;
    }

    // These UVC cams only expose Motion-JPEG, and three of them share one USB
    // controller — the compressed stream is what lets all three fit the bus
    // bandwidth. Must be set before the resolution, and only the V4L2 backend
    // honours CAP_PROP_FOURCC (GStreamer ignores it).
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH,  width_);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
    cap.set(cv::CAP_PROP_FPS,          fps_);
    // Bound in-driver queueing to ~1 frame so cap.read() returns the FRESHEST
    // frame, not one several deep — otherwise heavy downstream processing lets
    // the V4L2 driver build a backlog and the detector works on stale frames.
    // Set after the format/resolution changes (some drivers reset props on
    // format change).  Belt-and-suspenders with DetectionService's
    // latest-frame-wins worker, which guarantees freshness even if a driver
    // ignores BUFFERSIZE.
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);

    const int cc = static_cast<int>(cap.get(cv::CAP_PROP_FOURCC));
    const char fourcc[5] = { static_cast<char>(cc & 0xFF),
                             static_cast<char>((cc >> 8)  & 0xFF),
                             static_cast<char>((cc >> 16) & 0xFF),
                             static_cast<char>((cc >> 24) & 0xFF), 0 };
    std::cout << "[CameraCapture] cam" << cam_id_ << " opened ("
              << static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH))  << "x"
              << static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)) << " @ "
              << static_cast<int>(cap.get(cv::CAP_PROP_FPS))          << " fps, "
              << fourcc << ")\n";

    const auto start_time = std::chrono::steady_clock::now();
    cv::Mat frame;

    int consecutive_failures = 0;
    constexpr int MAX_FAILURES = 10;

    while (running_) {
        if (!cap.read(frame) || frame.empty()) {
            ++consecutive_failures;
            std::cerr << "[CameraCapture] cam" << cam_id_ << ": read failed ("
                      << consecutive_failures << "/" << MAX_FAILURES << ")\n";
            if (consecutive_failures >= MAX_FAILURES) {
                std::cerr << "[CameraCapture] cam" << cam_id_
                          << ": too many failures, stopping\n";
                running_ = false;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        consecutive_failures = 0;

        const auto now = std::chrono::steady_clock::now();
        const int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now - start_time).count();

        callback_({ cam_id_, frame.clone(), ms });
    }
}

} // namespace camstream
