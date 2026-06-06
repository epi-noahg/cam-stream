// camdetect_calibrate
// ─────────────────────────────────────────────────────────────────────────────
// Interactive calibration tool.
//
// Opens a video file, lets the user pick a representative frame with a
// trackbar, then click 4 reference points on the dartboard:
//
//   1. Bullseye centre
//   2. Outer double wire at sector 20 (top)
//   3. Outer double wire at sector  6 (right, 90° clockwise from 20)
//   4. Outer double wire at sector  3 (bottom)
//
// Once 4 points are placed, the homography is computed and a preview of the
// reprojected board rings is drawn on the frame.  Press 's' to save, 'r' to
// reset, 'q' to quit.
//
// Usage:
//   camdetect_calibrate <video.mp4> <output_calib.yml>

#include "camdetect/BoardCalibration.hpp"
#include "camdetect/BoardCalibrator.hpp"
#include "camdetect/Types.hpp"

#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <vector>

using namespace camdetect;

namespace {

constexpr const char* WINDOW = "camdetect calibrate";

const std::array<const char*, 4> POINT_LABELS = {
    "1: outer DOUBLE wire @ sector 20 (top,    12 o'clock)",
    "2: outer DOUBLE wire @ sector 6  (right,   3 o'clock)",
    "3: outer DOUBLE wire @ sector 3  (bottom,  6 o'clock)",
    "4: outer DOUBLE wire @ sector 11 (left,    9 o'clock)"
};

struct State {
    cv::Mat                   base_frame;     // current trackbar frame, untouched
    std::vector<cv::Point2f>  clicks;
    int                       frame_index{0};
    cv::VideoCapture          cap;
    int                       total_frames{0};
};

void onMouse(int event, int x, int y, int /*flags*/, void* userdata)
{
    if (event != cv::EVENT_LBUTTONDOWN) return;
    auto* st = static_cast<State*>(userdata);
    if (st->clicks.size() >= 4) return;
    st->clicks.emplace_back(static_cast<float>(x), static_cast<float>(y));
}

void onTrackbar(int pos, void* userdata)
{
    auto* st = static_cast<State*>(userdata);
    st->cap.set(cv::CAP_PROP_POS_FRAMES, pos);
    cv::Mat f;
    if (st->cap.read(f) && !f.empty()) {
        st->base_frame = f;
        st->frame_index = pos;
    }
}

void drawHud(cv::Mat& img, const State& st, const BoardCalibration* calib)
{
    // Already-placed clicks
    for (size_t i = 0; i < st.clicks.size(); ++i) {
        cv::circle(img, st.clicks[i], 6, {0, 255, 0}, 2);
        cv::putText(img,
                    POINT_LABELS[i],
                    st.clicks[i] + cv::Point2f(10.f, -10.f),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    {0, 255, 0}, 1);
    }

    // Next-point prompt
    if (st.clicks.size() < 4) {
        std::string msg = "Click " + std::string(POINT_LABELS[st.clicks.size()]);
        cv::putText(img, msg, {10, 25},
                    cv::FONT_HERSHEY_SIMPLEX, 0.6,
                    {0, 255, 255}, 2);
    }

    // Calibration preview: reproject board rings + sector lines
    if (calib && calib->isValid()) {
        const auto drawCircle = [&](float radius_mm, cv::Scalar color) {
            std::vector<cv::Point> pts;
            pts.reserve(72);
            for (int i = 0; i < 72; ++i) {
                const float a = i * (2.f * static_cast<float>(M_PI) / 72.f);
                const cv::Point2f bp{ radius_mm * std::sin(a),
                                      radius_mm * std::cos(a) };
                const auto ip = calib->boardToImage(bp);
                pts.emplace_back(cv::Point(cvRound(ip.x), cvRound(ip.y)));
            }
            const cv::Point* p = pts.data();
            const int        n = static_cast<int>(pts.size());
            cv::polylines(img, &p, &n, 1, true, color, 1, cv::LINE_AA);
        };

        drawCircle(board::BULLSEYE_RADIUS, {  0, 255, 255});
        drawCircle(board::BULL_RADIUS,     {  0, 200, 255});
        drawCircle(board::TRIPLE_INNER,    {  0, 255,   0});
        drawCircle(board::TRIPLE_OUTER,    {  0, 255,   0});
        drawCircle(board::DOUBLE_INNER,    {255, 100,   0});
        drawCircle(board::DOUBLE_OUTER,    {255, 100,   0});

        // 20 sector wires from inner-bull to outer-double
        for (int s = 0; s < 20; ++s) {
            const float a = (s * 18.f - 9.f) * static_cast<float>(M_PI) / 180.f;
            const cv::Point2f p_in {
                board::BULL_RADIUS    * std::sin(a),
                board::BULL_RADIUS    * std::cos(a)
            };
            const cv::Point2f p_out{
                board::DOUBLE_OUTER   * std::sin(a),
                board::DOUBLE_OUTER   * std::cos(a)
            };
            cv::line(img,
                     calib->boardToImage(p_in),
                     calib->boardToImage(p_out),
                     {200, 200, 200}, 1, cv::LINE_AA);
        }

        cv::putText(img, "Calibration OK — press 's' to save",
                    {10, img.rows - 15},
                    cv::FONT_HERSHEY_SIMPLEX, 0.55,
                    {0, 255, 0}, 2);
    }

    // Controls help
    cv::putText(img, "r: reset  s: save  q: quit",
                {10, img.rows - 40},
                cv::FONT_HERSHEY_SIMPLEX, 0.45,
                {200, 200, 200}, 1);
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <video.mp4> <output_calib.yml>\n";
        return 1;
    }
    const std::string video_path = argv[1];
    const std::string out_path   = argv[2];

    State st;
    st.cap.open(video_path);
    if (!st.cap.isOpened()) {
        std::cerr << "Cannot open video: " << video_path << '\n';
        return 1;
    }
    st.total_frames = static_cast<int>(st.cap.get(cv::CAP_PROP_FRAME_COUNT));

    // Read first frame to learn size
    if (!st.cap.read(st.base_frame) || st.base_frame.empty()) {
        std::cerr << "Cannot read first frame\n";
        return 1;
    }
    st.cap.set(cv::CAP_PROP_POS_FRAMES, 0);

    cv::namedWindow(WINDOW, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(WINDOW, onMouse, &st);
    if (st.total_frames > 1) {
        cv::createTrackbar("frame", WINDOW, nullptr,
                           std::max(1, st.total_frames - 1),
                           onTrackbar, &st);
    }

    BoardCalibrator   calibrator;
    BoardCalibration  calib;

    while (true) {
        // Recompute calibration whenever we have 4 clicks
        if (st.clicks.size() == 4 && !calib.isValid()) {
            calibrator.fromReferencePoints(st.clicks,
                                            st.base_frame.cols,
                                            st.base_frame.rows,
                                            calib);
        }

        cv::Mat display = st.base_frame.clone();
        drawHud(display, st, calib.isValid() ? &calib : nullptr);
        cv::imshow(WINDOW, display);

        const int key = cv::waitKey(20) & 0xff;
        if (key == 'q' || key == 27) {
            std::cout << "[calibrate] Aborted.\n";
            return 0;
        }
        if (key == 'r') {
            st.clicks.clear();
            calib = BoardCalibration{};
        }
        if (key == 's') {
            if (!calib.isValid()) {
                std::cerr << "[calibrate] Need 4 clicks before saving.\n";
                continue;
            }
            if (!calib.saveToFile(out_path)) {
                std::cerr << "[calibrate] Failed to save to " << out_path << '\n';
                return 1;
            }
            std::cout << "[calibrate] Saved → " << out_path << '\n';
            return 0;
        }
    }
}
