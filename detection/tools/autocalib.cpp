// camdetect_autocalib
// ─────────────────────────────────────────────────────────────────────────────
// Automatic board calibration: finds the rings/sectors from the image colours
// and produces BOTH a classic calibration yml (drop-in for the old system)
// and a pixel-accurate zone map png used for exact scoring.
//
// Interactive mode (default): pick a clean frame with the trackbar, the
// detection runs automatically.  Verify the coloured blobs / labels overlay;
// if the sector numbering is rotated, click anywhere inside sector 20 (its
// red double/triple) or nudge with 'o'/'p'.  Press 's' to save.
//
//   keys:  a  re-run detection on the current frame
//          o/p  rotate sector assignment by ∓1
//          click  mark the clicked sector as "20"
//          v  cycle view: overlay → masks → original
//          r  clear manual orientation tweaks
//          s  save yml + zones png      q  quit
//
// Batch mode (--batch): no UI, runs on --frame N (default 0), saves the yml,
// the zone map png and an _overlay.png for offline inspection.
//
// Usage:
//   camdetect_autocalib <video|image> <out_calib.yml> [--batch] [--frame N]
//
// The zone map is written next to the yml: <out_calib>_zones.png

#include "camdetect/AutoCalibrator.hpp"
#include "camdetect/ZoneMap.hpp"

#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <string>

using namespace camdetect;

namespace {

constexpr const char* WINDOW = "camdetect autocalib";

struct State {
    cv::Mat                 base_frame;
    cv::VideoCapture        cap;
    int                     total_frames {0};
    AutoCalibrator::Options opt;
    AutoCalibrator::Result  result;
    bool                    dirty    {true};   // re-run detection
    int                     view     {0};      // 0 overlay, 1 masks, 2 original
    bool                    tuning   {false};  // auto-tune in progress
};

void onTrackbarFrame(int pos, void* userdata)
{
    auto* st = static_cast<State*>(userdata);
    st->cap.set(cv::CAP_PROP_POS_FRAMES, pos);
    cv::Mat f;
    if (st->cap.read(f) && !f.empty()) {
        st->base_frame = f;
        st->dirty      = true;
    }
}

void onTrackbarThresh(int, void* userdata)
{
    static_cast<State*>(userdata)->dirty = true;
}

void onMouse(int event, int x, int y, int /*flags*/, void* userdata)
{
    if (event != cv::EVENT_LBUTTONDOWN) return;
    auto* st = static_cast<State*>(userdata);
    st->opt.sector20_hint   = {static_cast<float>(x), static_cast<float>(y)};
    st->opt.sector20_offset = 0;
    st->dirty               = true;
}

cv::Mat render(const State& st)
{
    const auto& r = st.result;
    cv::Mat display;

    if (st.view == 1 && !r.red_mask.empty()) {
        display = cv::Mat::zeros(st.base_frame.size(), CV_8UC3);
        display.setTo(cv::Scalar(0, 0, 255), r.red_mask);
        display.setTo(cv::Scalar(0, 255, 0), r.green_mask);
    } else if (st.view == 0 && r.ok) {
        display = r.zone_map.overlay(st.base_frame);
    } else {
        display = st.base_frame.clone();
    }

    const auto line = [&](int n, const std::string& s, cv::Scalar c) {
        cv::putText(display, s, {10, 22 + n * 20},
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 0}, 3, cv::LINE_AA);
        cv::putText(display, s, {10, 22 + n * 20},
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, c, 1, cv::LINE_AA);
    };

    if (st.tuning) {
        line(0, "AUTO-TUNE running...", {0, 200, 255});
    } else if (r.ok) {
        line(0, "T:" + std::to_string(r.triples_found) + "/20  D:" +
                std::to_string(r.doubles_found) + "/20  reproj:" +
                cv::format("%.1fpx", r.mean_reproj_err_px),
             r.warning.empty() ? cv::Scalar(0, 255, 0)
                               : cv::Scalar(0, 200, 255));
        if (!r.warning.empty()) line(1, r.warning, {0, 200, 255});
        line(2, "check the numbers; click inside sector 20 if rotated",
             {200, 200, 200});
    } else {
        line(0, "FAILED: " + r.error, {0, 0, 255});
    }
    line(3, "a:rerun  o/p:rotate  v:view  r:reset  t:tune  s:save  q:quit",
         {200, 200, 200});
    return display;
}

bool saveAll(const AutoCalibrator::Result& r, const std::string& yml_path,
             const cv::Mat& frame, bool write_overlay)
{
    const std::string zones_path = ZoneMap::companionPath(yml_path);
    if (!r.calibration.saveToFile(yml_path)) {
        std::cerr << "[autocalib] cannot write " << yml_path << '\n';
        return false;
    }
    if (!r.zone_map.saveToFile(zones_path)) {
        std::cerr << "[autocalib] cannot write " << zones_path << '\n';
        return false;
    }
    std::cout << "[autocalib] saved " << yml_path << " + " << zones_path << '\n';
    if (write_overlay) {
        const std::string stem = zones_path.substr(0, zones_path.size() - 4);
        cv::imwrite(stem + "_overlay.png", r.zone_map.overlay(frame));
        cv::imwrite(stem + "_red.png",     r.red_mask);
        cv::imwrite(stem + "_green.png",   r.green_mask);
        std::cout << "[autocalib] overlay for inspection: "
                  << stem + "_overlay.png" << '\n';
    }
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    std::string input, out_yml;
    bool        batch    = false;
    bool        autotune = false;
    int         frame_idx = 0;
    int         rotate = 0;
    cv::Point2f st_hint{-1.f, -1.f};
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--batch")                       batch    = true;
        else if (a == "--autotune")               autotune = true;
        else if (a == "--frame" && i + 1 < argc)  frame_idx = std::stoi(argv[++i]);
        else if (a == "--hint" && i + 2 < argc) {
            st_hint.x = std::stof(argv[++i]);
            st_hint.y = std::stof(argv[++i]);
        }
        else if (a == "--rotate" && i + 1 < argc) rotate = std::stoi(argv[++i]);
        else if (input.empty())                   input = a;
        else if (out_yml.empty())                 out_yml = a;
    }
    if (input.empty() || out_yml.empty()) {
        std::cerr << "Usage: " << argv[0]
                  << " <video|image> <out_calib.yml> [--batch] [--frame N]"
                  << " [--hint X Y] [--rotate K] [--autotune]\n"
                  << "  --hint X Y   pixel inside sector 20 (orientation)\n"
                  << "  --rotate K   rotate sector assignment by K\n"
                  << "  --autotune   sweep colour thresholds for best result\n";
        return 1;
    }

    State st;
    // Try as image first, then as video.
    st.base_frame = cv::imread(input);
    if (st.base_frame.empty()) {
        st.cap.open(input);
        if (!st.cap.isOpened()) {
            std::cerr << "Cannot open input: " << input << '\n';
            return 1;
        }
        st.total_frames = static_cast<int>(st.cap.get(cv::CAP_PROP_FRAME_COUNT));
        st.cap.set(cv::CAP_PROP_POS_FRAMES, frame_idx);
        if (!st.cap.read(st.base_frame) || st.base_frame.empty()) {
            std::cerr << "Cannot read frame " << frame_idx << '\n';
            return 1;
        }
    }

    st.opt.sector20_hint   = st_hint;
    st.opt.sector20_offset = rotate;

    const AutoCalibrator calibrator;

    if (batch) {
        if (autotune) {
            std::cout << "[autocalib] auto-tuning colour thresholds...\n";
            st.opt = calibrator.tune(st.base_frame, st.opt);
            std::cout << "[autocalib] best params: red_a=" << st.opt.red_a_delta
                      << " green_a=" << st.opt.green_a_delta
                      << " chroma=" << st.opt.min_chroma << '\n';
        }
        st.result = calibrator.run(st.base_frame, st.opt);
        if (!st.result.ok) {
            std::cerr << "[autocalib] FAILED: " << st.result.error << '\n';
            // Dump the colour masks so the failure can be diagnosed offline.
            const std::string stem = ZoneMap::companionPath(out_yml);
            if (!st.result.red_mask.empty()) {
                cv::imwrite(stem.substr(0, stem.size() - 4) + "_red.png",
                            st.result.red_mask);
                cv::imwrite(stem.substr(0, stem.size() - 4) + "_green.png",
                            st.result.green_mask);
            }
            return 1;
        }
        std::cout << "[autocalib] triples " << st.result.triples_found
                  << "/20, doubles " << st.result.doubles_found
                  << "/20, reproj err "
                  << cv::format("%.2f", st.result.mean_reproj_err_px) << "px\n";
        if (!st.result.warning.empty())
            std::cout << "[autocalib] warning: " << st.result.warning << '\n';
        return saveAll(st.result, out_yml, st.base_frame, true) ? 0 : 1;
    }

    cv::namedWindow(WINDOW, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(WINDOW, onMouse, &st);
    if (st.total_frames > 1)
        cv::createTrackbar("frame", WINDOW, nullptr,
                           std::max(1, st.total_frames - 1),
                           onTrackbarFrame, &st);
    // Colour threshold tuning, for tricky lighting.
    cv::createTrackbar("red a*",   WINDOW, nullptr, 60, onTrackbarThresh, &st);
    cv::createTrackbar("green a*", WINDOW, nullptr, 60, onTrackbarThresh, &st);
    cv::createTrackbar("chroma",   WINDOW, nullptr, 60, onTrackbarThresh, &st);
    cv::setTrackbarPos("red a*",   WINDOW, st.opt.red_a_delta);
    cv::setTrackbarPos("green a*", WINDOW, st.opt.green_a_delta);
    cv::setTrackbarPos("chroma",   WINDOW, st.opt.min_chroma);

    while (true) {
        if (st.dirty && !st.tuning) {
            st.opt.red_a_delta   = std::max(4, cv::getTrackbarPos("red a*",   WINDOW));
            st.opt.green_a_delta = std::max(4, cv::getTrackbarPos("green a*", WINDOW));
            st.opt.min_chroma    = std::max(6, cv::getTrackbarPos("chroma",   WINDOW));
            st.result = calibrator.run(st.base_frame, st.opt);
            st.dirty  = false;
            if (!st.result.ok)
                std::cerr << "[autocalib] " << st.result.error << '\n';
        }

        cv::imshow(WINDOW, render(st));
        const int key = cv::waitKey(30) & 0xff;
        if (key == 'q' || key == 27) return 0;
        if (key == 'a') st.dirty = true;
        if (key == 'v') st.view = (st.view + 1) % 3;
        if (key == 'o') { --st.opt.sector20_offset; st.dirty = true; }
        if (key == 'p') { ++st.opt.sector20_offset; st.dirty = true; }
        if (key == 'r') {
            st.opt.sector20_offset = 0;
            st.opt.sector20_hint   = {-1.f, -1.f};
            st.dirty = true;
        }
        if (key == 't') {
            st.tuning = true;
            cv::imshow(WINDOW, render(st));
            cv::waitKey(1);
            std::cout << "[autocalib] auto-tuning colour thresholds...\n";
            st.opt = calibrator.tune(st.base_frame, st.opt);
            std::cout << "[autocalib] best: red_a=" << st.opt.red_a_delta
                      << " green_a=" << st.opt.green_a_delta
                      << " chroma=" << st.opt.min_chroma << '\n';
            cv::setTrackbarPos("red a*",   WINDOW, st.opt.red_a_delta);
            cv::setTrackbarPos("green a*", WINDOW, st.opt.green_a_delta);
            cv::setTrackbarPos("chroma",   WINDOW, st.opt.min_chroma);
            st.tuning = false;
            st.dirty  = true;
        }
        if (key == 's') {
            if (!st.result.ok) {
                std::cerr << "[autocalib] nothing valid to save\n";
                continue;
            }
            if (saveAll(st.result, out_yml, st.base_frame, true)) return 0;
        }
    }
}
