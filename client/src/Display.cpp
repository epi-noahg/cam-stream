#include "Display.hpp"

#include <string>

namespace camstream {

Display::Display(int num_cameras, int cam_width, int cam_height)
    : num_cameras_(num_cameras)
    , cam_width_(cam_width)
    , cam_height_(cam_height)
    , frames_(static_cast<size_t>(num_cameras))
{
    // Allocate one mutex per camera (std::mutex is non-copyable/non-movable)
    mutexes_.reserve(static_cast<size_t>(num_cameras));
    for (int i = 0; i < num_cameras; ++i)
        mutexes_.push_back(new std::mutex());

    cv::namedWindow(window_name_, cv::WINDOW_AUTOSIZE);
}

Display::~Display()
{
    cv::destroyAllWindows();
    for (auto* m : mutexes_) delete m;
}

void Display::updateFrame(int cam_id, cv::Mat frame)
{
    if (cam_id < 0 || cam_id >= num_cameras_) return;
    std::lock_guard<std::mutex> lk(*mutexes_[static_cast<size_t>(cam_id)]);
    frames_[static_cast<size_t>(cam_id)] = std::move(frame);
}

bool Display::render()
{
    // Build a composite image: all feeds placed side-by-side
    cv::Mat composite(cam_height_, cam_width_ * num_cameras_, CV_8UC3,
                      cv::Scalar(20, 20, 20));

    for (int i = 0; i < num_cameras_; ++i) {
        cv::Mat frame;
        {
            std::lock_guard<std::mutex> lk(*mutexes_[static_cast<size_t>(i)]);
            if (!frames_[static_cast<size_t>(i)].empty())
                frame = frames_[static_cast<size_t>(i)].clone();
        }

        cv::Rect roi(i * cam_width_, 0, cam_width_, cam_height_);
        if (!frame.empty()) {
            // Resize in case the decoded frame dimensions differ slightly
            cv::Mat resized;
            cv::resize(frame, resized, cv::Size(cam_width_, cam_height_));
            resized.copyTo(composite(roi));
        }

        // Overlay camera label
        cv::putText(composite,
                    "CAM " + std::to_string(i),
                    cv::Point(i * cam_width_ + 10, 28),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8,
                    cv::Scalar(0, 230, 0), 2, cv::LINE_AA);
    }

    cv::imshow(window_name_, composite);

    // waitKey drives the GUI event loop on macOS; 33 ms ≈ 30 fps polling
    const int key = cv::waitKey(33);
    return (key != 27) && (cv::getWindowProperty(window_name_,
                                                  cv::WND_PROP_VISIBLE) > 0);
}

} // namespace camstream
