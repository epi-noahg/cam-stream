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
    cv::VideoCapture cap(device_);
    if (!cap.isOpened()) {
        std::cerr << "[CameraCapture] cam" << cam_id_
                  << ": cannot open " << device_ << "\n";
        running_ = false;
        return;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH,  width_);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
    cap.set(cv::CAP_PROP_FPS,          fps_);

    std::cout << "[CameraCapture] cam" << cam_id_ << " opened ("
              << static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH))  << "x"
              << static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)) << " @ "
              << static_cast<int>(cap.get(cv::CAP_PROP_FPS))          << " fps)\n";

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
