#include "Recorder.hpp"
#include "Protocol.hpp"   // MAX_CAMERAS

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace camstream {

// ── Constructor / destructor ──────────────────────────────────────────────────

Recorder::Recorder(std::string output_dir)
    : output_dir_(std::move(output_dir))
{
    writers_.reserve(MAX_CAMERAS);
    for (int i = 0; i < MAX_CAMERAS; ++i)
        writers_.push_back(new Writer{});
}

Recorder::~Recorder()
{
    close();
    for (auto* w : writers_) delete w;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool Recorder::open(int cam_id, int width, int height, int fps)
{
    if (cam_id < 0 || cam_id >= static_cast<int>(writers_.size())) return false;

    Writer& w = *writers_[static_cast<size_t>(cam_id)];
    std::lock_guard<std::mutex> lk(w.mtx);

    if (w.open) return true;   // already opened (e.g. duplicate Init packet)

    const std::string path = makeFilename(cam_id);

    // avc1 = H.264 in MP4 container – well supported on macOS and most players.
    // Fall back to mp4v if avc1 is unavailable on the current platform.
    const int fourcc = cv::VideoWriter::fourcc('a', 'v', 'c', '1');

    if (!w.vw.open(path, fourcc, fps, cv::Size(width, height))) {
        std::cerr << "[Recorder] Failed to open output file: " << path << "\n";
        return false;
    }

    w.open = true;
    std::cout << "[Recorder] cam" << cam_id << " → " << path << "\n";
    return true;
}

void Recorder::writeFrame(int cam_id, const cv::Mat& frame)
{
    if (cam_id < 0 || cam_id >= static_cast<int>(writers_.size())) return;

    Writer& w = *writers_[static_cast<size_t>(cam_id)];
    std::lock_guard<std::mutex> lk(w.mtx);

    if (w.open && !frame.empty())
        w.vw.write(frame);
}

void Recorder::close()
{
    for (auto* w : writers_) {
        std::lock_guard<std::mutex> lk(w->mtx);
        if (w->open) {
            w->vw.release();
            w->open = false;
        }
    }
    std::cout << "[Recorder] All files closed\n";
}

bool Recorder::isOpen(int cam_id) const
{
    if (cam_id < 0 || cam_id >= static_cast<int>(writers_.size())) return false;
    return writers_[static_cast<size_t>(cam_id)]->open;
}

// ── Private helpers ───────────────────────────────────────────────────────────

std::string Recorder::makeFilename(int cam_id) const
{
    // Timestamp: YYYYMMDD_HHMMSS
    const std::time_t now = std::time(nullptr);
    const std::tm*    tm  = std::localtime(&now);

    std::ostringstream oss;
    oss << output_dir_ << "/cam" << cam_id << "_"
        << std::put_time(tm, "%Y%m%d_%H%M%S")
        << ".mp4";
    return oss.str();
}

} // namespace camstream
