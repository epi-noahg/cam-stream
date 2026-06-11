#include "camdetect/DebugUI.hpp"
#include "camdetect/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>

namespace camdetect {

namespace {

// ── Modernised palette (all BGR) ───────────────────────────────────────────
constexpr int   INFO_PANEL_W   = 300;
const cv::Scalar BG_PRIMARY    (24, 22, 20);     // deep slate
const cv::Scalar BG_PANEL      (40, 36, 32);
const cv::Scalar BG_HEADER     (60, 54, 48);
const cv::Scalar TEXT_PRIMARY  (235, 235, 235);
const cv::Scalar TEXT_DIM      (160, 160, 160);
const cv::Scalar TEXT_FAINT    (110, 110, 110);
const cv::Scalar ACCENT_OK     (140, 220, 120);
const cv::Scalar ACCENT_WARN   (90, 180, 255);
const cv::Scalar ACCENT_ERR    (90,  90, 220);
const cv::Scalar ACCENT_INFO   (255, 200, 120);
const cv::Scalar DIVIDER       (80,  72,  64);

const std::array<cv::Scalar, 3> DART_COLORS = {
    cv::Scalar( 80,  80, 255),   // dart 0 = red    (BGR)
    cv::Scalar( 80, 220, 255),   // dart 1 = yellow
    cv::Scalar( 80, 255,  80)    // dart 2 = green
};

const std::array<cv::Scalar, NUM_CAMS> CAM_COLORS = {
    cv::Scalar(255, 140, 100),   // cam0 blue-ish
    cv::Scalar(100, 255, 140),   // cam1 green
    cv::Scalar(140, 100, 255)    // cam2 pink
};

float pairwiseSpread(const FusedHit& h)
{
    float spread = 0.f;
    for (int a = 0; a < NUM_CAMS; ++a)
        for (int b = a + 1; b < NUM_CAMS; ++b) {
            if (h.per_cam[a].cam_id < 0 || h.per_cam[b].cam_id < 0) continue;
            const auto d = h.per_cam[a].board_xy - h.per_cam[b].board_xy;
            spread = std::max(spread, std::sqrt(d.x*d.x + d.y*d.y));
        }
    return spread;
}

// Aspect-fit `src` into `dst_roi` of `out`.  Letterboxes with `pad_col`.
void blitAspectFit(cv::Mat& out, const cv::Rect& dst_roi, const cv::Mat& src,
                   const cv::Scalar& pad_col)
{
    if (src.empty() || dst_roi.width <= 0 || dst_roi.height <= 0) return;
    cv::Mat tile = out(dst_roi);
    tile.setTo(pad_col);

    const float src_aspect = float(src.cols) / float(src.rows);
    const float dst_aspect = float(dst_roi.width) / float(dst_roi.height);
    int tw, th;
    if (src_aspect > dst_aspect) {
        tw = dst_roi.width;
        th = std::max(1, int(std::round(dst_roi.width / src_aspect)));
    } else {
        th = dst_roi.height;
        tw = std::max(1, int(std::round(dst_roi.height * src_aspect)));
    }
    const int ox = (dst_roi.width  - tw) / 2;
    const int oy = (dst_roi.height - th) / 2;
    const int flag = (src.cols > tw || src.rows > th) ? cv::INTER_AREA
                                                      : cv::INTER_CUBIC;
    cv::Mat resized;
    cv::resize(src, resized, {tw, th}, 0, 0, flag);
    resized.copyTo(tile(cv::Rect(ox, oy, tw, th)));
}

} // anon

// ── Mouse plumbing ─────────────────────────────────────────────────────────
namespace {
void onMouseStatic(int event, int x, int y, int /*flags*/, void* userdata)
{
    if (event != cv::EVENT_LBUTTONDOWN) return;
    static_cast<DebugUI*>(userdata)->handleClick(x, y);
}
} // anon

DebugUI::DebugUI(std::array<BoardCalibration, NUM_CAMS> calibs,
                 int cam_tile_width,
                 int cam_tile_height,
                 int board_view_size)
    : calibs_(std::move(calibs)),
      tile_w_(cam_tile_width),
      tile_h_(cam_tile_height),
      board_size_(board_view_size)
{
    cv::namedWindow(WINDOW_NAME, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(WINDOW_NAME, onMouseStatic, this);
}

DebugUI::~DebugUI()
{
    cv::destroyWindow(WINDOW_NAME);
}

void DebugUI::updateCam(int cam_id, const cv::Mat& frame, const DetectorViz& viz)
{
    if (cam_id < 0 || cam_id >= NUM_CAMS) return;
    std::lock_guard<std::mutex> lk(mtx_);
    frames_[cam_id] = frame.clone();
    vizs_[cam_id]   = viz;
}

void DebugUI::setZoneMap(int cam_id, const ZoneMap& zm)
{
    if (cam_id < 0 || cam_id >= NUM_CAMS || zm.empty()) return;
    const cv::Mat& labels = zm.labels();

    cv::Mat color(labels.size(), CV_8UC3, cv::Scalar(0, 0, 0));
    std::map<uint16_t, cv::Vec3b> palette;
    for (int y = 0; y < labels.rows; ++y) {
        const auto* lr = labels.ptr<uint16_t>(y);
        auto*       cr = color.ptr<cv::Vec3b>(y);
        for (int x = 0; x < labels.cols; ++x) {
            const uint16_t id = lr[x];
            if (id == ZoneMap::MISS) continue;
            auto it = palette.find(id);
            if (it == palette.end()) {
                const cv::Scalar c = ZoneMap::idColor(id);
                it = palette.emplace(id, cv::Vec3b(uchar(c[0]), uchar(c[1]),
                                                   uchar(c[2]))).first;
            }
            cr[x] = it->second;
        }
    }
    cv::Mat gx, gy;
    cv::Sobel(labels, gx, CV_32F, 1, 0, 1);
    cv::Sobel(labels, gy, CV_32F, 0, 1, 1);

    std::lock_guard<std::mutex> lk(mtx_);
    zone_color_[cam_id] = color;
    zone_area_[cam_id]  = labels > 0;
    zone_edges_[cam_id] = (cv::abs(gx) + cv::abs(gy)) > 0;
}

void DebugUI::setRoundHits(std::vector<FusedHit> hits)
{
    std::lock_guard<std::mutex> lk(mtx_);
    round_hits_ = std::move(hits);
}

void DebugUI::setRoundProgress(int darts_in_round, int round_total_score)
{
    std::lock_guard<std::mutex> lk(mtx_);
    darts_in_round_    = darts_in_round;
    round_total_score_ = round_total_score;
}

void DebugUI::setDiffThreshold(float v)
{
    std::lock_guard<std::mutex> lk(mtx_);
    diff_threshold_ = v;
}

void DebugUI::setCamDelay(int cam_id, int frames)
{
    if (cam_id < 0 || cam_id >= NUM_CAMS) return;
    std::lock_guard<std::mutex> lk(mtx_);
    cam_delays_[cam_id] = frames;
}

void DebugUI::setRoundStatus(const std::string& message, int phase_index)
{
    std::lock_guard<std::mutex> lk(mtx_);
    round_status_msg_ = message;
    round_phase_      = phase_index;
}

bool DebugUI::consumeResetRequest()
{
    std::lock_guard<std::mutex> lk(mtx_);
    bool v = reset_requested_;
    reset_requested_ = false;
    return v;
}

bool DebugUI::consumeBgRefreshRequest()
{
    std::lock_guard<std::mutex> lk(mtx_);
    bool v = bg_refresh_requested_;
    bg_refresh_requested_ = false;
    return v;
}

bool DebugUI::consumeStepForward()
{
    std::lock_guard<std::mutex> lk(mtx_);
    bool v = step_forward_;
    step_forward_ = false;
    return v;
}

bool DebugUI::consumeStepBackward()
{
    std::lock_guard<std::mutex> lk(mtx_);
    bool v = step_backward_;
    step_backward_ = false;
    return v;
}

bool DebugUI::isPaused() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return paused_;
}

int DebugUI::focusedCam() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return focused_cam_id_;
}

void DebugUI::handleClick(int x, int y)
{
    std::lock_guard<std::mutex> lk(mtx_);
    for (int i = 0; i < NUM_CAMS; ++i) {
        if (last_tile_rois_[i].contains(cv::Point(x, y))) {
            // Click on the already-focused cam → unfocus.  Otherwise → focus it.
            focused_cam_id_ = (focused_cam_id_ == i) ? -1 : i;
            return;
        }
    }
}

// ── Tile renderer (shared between grid + focus modes) ─────────────────────
void DebugUI::drawCamTile(cv::Mat& out, const cv::Rect& roi, int cam_id,
                          bool draw_label, float font_scale) const
{
    last_tile_rois_[cam_id] = roi;
    if (roi.width <= 0 || roi.height <= 0) return;

    cv::Mat tile = out(roi);
    tile.setTo(BG_PANEL);

    if (frames_[cam_id].empty()) {
        cv::putText(tile, "cam" + std::to_string(cam_id) + " (no signal)",
                    {15, 30}, cv::FONT_HERSHEY_DUPLEX, 0.55,
                    TEXT_DIM, 1, cv::LINE_AA);
        cv::rectangle(tile, {0, 0}, {tile.cols - 1, tile.rows - 1},
                      DIVIDER, 1, cv::LINE_AA);
        return;
    }

    // Build the overlay image (frame or mask) + dart/calibration drawings.
    cv::Mat overlay;
    if (show_mask_ && !vizs_[cam_id].mask.empty()) {
        cv::cvtColor(vizs_[cam_id].mask, overlay, cv::COLOR_GRAY2BGR);
    } else {
        overlay = frames_[cam_id].clone();
    }

    // Pixel-zone overlay ('z'): tint every scoring zone + hairline wire
    // boundaries, beneath the dart drawings so detections stay on top.
    if (show_zones_ && !zone_color_[cam_id].empty() &&
        zone_color_[cam_id].size() == overlay.size()) {
        cv::Mat blended;
        cv::addWeighted(overlay, 0.65, zone_color_[cam_id], 0.35, 0, blended);
        blended.copyTo(overlay, zone_area_[cam_id]);
        overlay.setTo(cv::Scalar(230, 230, 230), zone_edges_[cam_id]);
    }

    // Tint + bounding box around every dart-shaped region.  The detector
    // unions the band-masks of all elongated contours (including darts it
    // already committed) so we draw a BB per connected component — i.e. one
    // box per visible dart.  Yellow at line thickness 3 so it stands out on
    // both color and mask views.
    if (!vizs_[cam_id].dart_region.empty() &&
        vizs_[cam_id].dart_region.size() == overlay.size()) {

        // Green tint of the included pixels
        cv::Mat layer(overlay.size(), overlay.type(),
                      cv::Scalar(60, 230, 90));
        cv::Mat blended;
        cv::addWeighted(overlay, 0.45, layer, 0.55, 0, blended);
        blended.copyTo(overlay, vizs_[cam_id].dart_region);

        // Per-dart rotated bounding box
        cv::Mat labels;
        const int n_comp = cv::connectedComponents(
            vizs_[cam_id].dart_region, labels, 8, CV_32S);
        for (int k = 1; k < n_comp; ++k) {
            cv::Mat comp;
            cv::compare(labels, k, comp, cv::CMP_EQ);
            std::vector<cv::Point> nz;
            cv::findNonZero(comp, nz);
            if (nz.size() < 50) continue;   // skip tiny noise components
            const cv::RotatedRect bb = cv::minAreaRect(nz);
            cv::Point2f corners[4];
            bb.points(corners);
            for (int j = 0; j < 4; ++j) {
                cv::line(overlay, corners[j], corners[(j + 1) % 4],
                         cv::Scalar(0, 255, 255), 3, cv::LINE_AA);
            }
        }
    }

    if (calibs_[cam_id].isValid())
        Renderer::drawCalibrationOverlay(overlay, calibs_[cam_id]);
    for (const auto& lt : vizs_[cam_id].logged_tips_px)
        Renderer::drawLoggedTip(overlay, lt, CAM_COLORS[cam_id]);
    if (vizs_[cam_id].has_detection) {
        Renderer::drawDetectionOverlay(overlay,
                                        vizs_[cam_id].contour,
                                        vizs_[cam_id].tip_pixel,
                                        vizs_[cam_id].dart_dir);
        Renderer::drawFullDart(overlay,
                                vizs_[cam_id].tip_pixel,
                                vizs_[cam_id].tail_pixel);
    }

    blitAspectFit(out, roi, overlay, cv::Scalar(0, 0, 0));

    // ── Label bar at top of tile ─────────────────────────────────────────
    if (draw_label) {
        const int bar_h = std::max(22, int(28 * font_scale / 0.55f));
        cv::Rect bar(roi.x, roi.y, roi.width, bar_h);
        cv::Mat bar_roi = out(bar);
        cv::addWeighted(bar_roi, 0.0, bar_roi, 0.0, 0, bar_roi);  // clear
        bar_roi.setTo(cv::Scalar(0, 0, 0));
        // semi-transparent fade
        cv::Mat fade(bar.height, bar.width, CV_8UC3, BG_HEADER);
        cv::addWeighted(fade, 0.65, out(bar), 0.35, 0, out(bar));

        const char* state_str = "";
        cv::Scalar state_col = TEXT_DIM;
        switch (vizs_[cam_id].state) {
            case DetectorState::Warmup:
                state_str = " WARMUP";  state_col = ACCENT_WARN; break;
            case DetectorState::HumanBlob:
                state_str = " HUMAN";   state_col = ACCENT_ERR;  break;
            case DetectorState::BoardClean:
                state_str = " CLEAN";   state_col = ACCENT_OK;   break;
            case DetectorState::Normal:
                state_str = "";                                  break;
        }

        std::ostringstream lbl;
        lbl << "cam" << cam_id
            << "  t=" << std::fixed << std::setprecision(2)
            << vizs_[cam_id].timestamp << "s"
            << "  log=" << vizs_[cam_id].logged_tips_px.size();
        if (cam_delays_[cam_id] != 0) {
            lbl << "  d="
                << (cam_delays_[cam_id] > 0 ? "+" : "")
                << cam_delays_[cam_id] << "f";
        }

        cv::putText(out, lbl.str(), {roi.x + 10, roi.y + bar.height - 7},
                    cv::FONT_HERSHEY_DUPLEX, font_scale,
                    CAM_COLORS[cam_id], 1, cv::LINE_AA);
        if (state_str[0]) {
            const int tx = roi.x + roi.width - 100;
            cv::putText(out, state_str, {tx, roi.y + bar.height - 7},
                        cv::FONT_HERSHEY_DUPLEX, font_scale,
                        state_col, 1, cv::LINE_AA);
        }
    }

    // Cam-coloured outer border; brighter when focused.
    const bool is_focused = (focused_cam_id_ == cam_id);
    const int  thick      = is_focused ? 3 : 2;
    cv::rectangle(out, {roi.x, roi.y},
                  {roi.x + roi.width - 1, roi.y + roi.height - 1},
                  CAM_COLORS[cam_id], thick, cv::LINE_AA);
    if (is_focused) {
        // Inner accent ring
        cv::rectangle(out,
                      {roi.x + 3, roi.y + 3},
                      {roi.x + roi.width - 4, roi.y + roi.height - 4},
                      cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    }
}

// ── Main composite ─────────────────────────────────────────────────────────
void DebugUI::composite(cv::Mat& out) const
{
    const int total_w = tile_w_ * NUM_CAMS + INFO_PANEL_W;
    const int top_h   = tile_h_;
    const int bot_h   = board_size_;
    const int total_h = top_h + bot_h;
    out.create(total_h, total_w, CV_8UC3);
    out.setTo(BG_PRIMARY);

    // Reset hit-test ROIs every frame (focus mode reassigns them below).
    for (auto& r : last_tile_rois_) r = cv::Rect();

    if (focused_cam_id_ < 0) {
        // ── Grid mode: 3 tiles across the top ──────────────────────────────
        for (int i = 0; i < NUM_CAMS; ++i) {
            const cv::Rect roi(i * tile_w_, 0, tile_w_, tile_h_);
            drawCamTile(out, roi, i);
        }
    } else {
        // ── Focus mode: big focused cam on left, two thumbs on right strip ─
        const int focus_w = 2 * tile_w_;
        const int focus_h = total_h;            // full height
        const cv::Rect focus_roi(0, 0, focus_w, focus_h);
        drawCamTile(out, focus_roi, focused_cam_id_, /*draw_label*/ true, 0.75f);

        const int thumb_x = focus_w;
        const int thumb_w = tile_w_;
        const int thumb_h = tile_h_ / 2;
        int slot = 0;
        for (int i = 0; i < NUM_CAMS; ++i) {
            if (i == focused_cam_id_) continue;
            const cv::Rect roi(thumb_x, slot * thumb_h, thumb_w, thumb_h);
            drawCamTile(out, roi, i, /*draw_label*/ true, 0.45f);
            ++slot;
        }
    }

    // ── Canonical board view ───────────────────────────────────────────────
    cv::Mat board = Renderer::renderCanonicalBoard(board_size_);

    int best_idx = -1;
    float best_conf = -1.f;
    for (size_t i = 0; i < round_hits_.size(); ++i) {
        if (round_hits_[i].confidence > best_conf) {
            best_conf = round_hits_[i].confidence;
            best_idx  = static_cast<int>(i);
        }
    }

    for (size_t i = 0; i < round_hits_.size(); ++i) {
        const auto& h = round_hits_[i];
        const cv::Scalar col = DART_COLORS[std::min<size_t>(i, 2)];
        const cv::Point2f xy = h.board_xy;
        const std::string lbl = "#" + std::to_string(i + 1) + " " + h.zone;
        Renderer::drawHitOnCanonical(board, xy, col, lbl);
        if (static_cast<int>(i) == best_idx) {
            const float half  = board.cols * 0.5f;
            const float scale = (half * 0.92f) / board::DOUBLE_OUTER;
            const cv::Point p{
                cvRound(half + xy.x * scale),
                cvRound(half - xy.y * scale)
            };
            cv::circle(board, p, 14, {255, 255, 255}, 2, cv::LINE_AA);
            cv::circle(board, p, 16, col, 1, cv::LINE_AA);
        }
    }

    for (int i = 0; i < NUM_CAMS; ++i) {
        if (vizs_[i].has_detection) {
            Renderer::drawHitOnCanonical(board, vizs_[i].board_xy,
                                          CAM_COLORS[i],
                                          "c" + std::to_string(i));
        }
    }

    // Place board: bottom-left in grid mode; bottom-left of right strip in
    // focus mode (so it stays visible).
    cv::Rect board_roi;
    if (focused_cam_id_ < 0) {
        board_roi = cv::Rect(0, top_h, board_size_, board_size_);
    } else {
        const int strip_x = 2 * tile_w_;
        const int strip_w = tile_w_;
        const int strip_top = tile_h_;   // below the two thumbnails
        const int sz = std::min(strip_w, total_h - strip_top);
        board_roi = cv::Rect(strip_x + (strip_w - sz) / 2, strip_top, sz, sz);
    }
    if (board_roi.x + board_roi.width <= out.cols &&
        board_roi.y + board_roi.height <= out.rows) {
        cv::Mat board_resized;
        cv::resize(board, board_resized,
                   {board_roi.width, board_roi.height}, 0, 0, cv::INTER_AREA);
        board_resized.copyTo(out(board_roi));
    }

    // ── Info panel (right) ────────────────────────────────────────────────
    const int panel_x = NUM_CAMS * tile_w_;
    const cv::Rect panel_roi(panel_x, 0, INFO_PANEL_W, total_h);
    cv::Mat panel = out(panel_roi);
    panel.setTo(BG_PANEL);
    // Left divider
    cv::line(out, {panel_x, 0}, {panel_x, total_h}, DIVIDER, 1, cv::LINE_AA);

    int y = 32;
    const auto putLine = [&](const std::string& s,
                             cv::Scalar col = TEXT_PRIMARY,
                             double sc = 0.46,
                             int font = cv::FONT_HERSHEY_DUPLEX) {
        cv::putText(panel, s, {14, y}, font, sc, col, 1, cv::LINE_AA);
        y += static_cast<int>(20 + (sc - 0.46) * 14);
    };
    const auto putHeader = [&](const std::string& s) {
        // accent bar + uppercased label
        cv::rectangle(panel, {10, y - 14}, {12, y - 2}, ACCENT_INFO, -1);
        cv::putText(panel, s, {20, y - 4},
                    cv::FONT_HERSHEY_DUPLEX, 0.46, ACCENT_INFO, 1, cv::LINE_AA);
        y += 14;
    };
    const auto putDivider = [&]() {
        cv::line(panel, {14, y - 4}, {INFO_PANEL_W - 14, y - 4},
                 DIVIDER, 1, cv::LINE_AA);
        y += 8;
    };

    cv::putText(panel, "camdetect", {14, y},
                cv::FONT_HERSHEY_DUPLEX, 0.7, TEXT_PRIMARY, 1, cv::LINE_AA);
    cv::putText(panel, "debug", {130, y},
                cv::FONT_HERSHEY_DUPLEX, 0.55, ACCENT_INFO, 1, cv::LINE_AA);
    y += 18;
    putDivider();

    // ── Round status banner ───────────────────────────────────────────────
    {
        // Phase index: 0 = waiting dart, 1 = complete, 2 = resyncing
        cv::Scalar status_col;
        switch (round_phase_) {
            case 1:  status_col = ACCENT_OK;   break;  // collect
            case 2:  status_col = ACCENT_WARN; break;  // wait
            default: status_col = ACCENT_INFO; break;  // throw
        }
        const cv::Rect banner(8, y - 14, INFO_PANEL_W - 16, 44);
        cv::Mat banner_roi = panel(banner);
        cv::Mat fill(banner.height, banner.width, CV_8UC3,
                     status_col * 0.18);
        cv::addWeighted(fill, 1.0, banner_roi, 1.0, 0, banner_roi);
        cv::rectangle(panel, banner, status_col, 1, cv::LINE_AA);
        const std::string msg = round_status_msg_.empty()
                                    ? std::string("Initialising...")
                                    : round_status_msg_;
        cv::putText(panel, msg,
                    {18, y + 14},
                    cv::FONT_HERSHEY_DUPLEX, 0.55, status_col, 1, cv::LINE_AA);
        y += 50;
        putDivider();
    }

    putHeader("CALIBRATION");
    for (int i = 0; i < NUM_CAMS; ++i) {
        const bool ok = calibs_[i].isValid();
        std::ostringstream s;
        s << "  cam" << i << "  " << (ok ? "OK" : "MISSING");
        if (cam_delays_[i] != 0) {
            s << "   d=" << (cam_delays_[i] > 0 ? "+" : "")
              << cam_delays_[i] << "f";
        }
        putLine(s.str(), ok ? ACCENT_OK : ACCENT_ERR);
    }
    y += 4;
    putDivider();

    putHeader("DETECTION");
    {
        std::ostringstream s;
        s << "  threshold  " << std::fixed << std::setprecision(1)
          << diff_threshold_;
        putLine(s.str());
    }
    if (focused_cam_id_ >= 0) {
        std::ostringstream s;
        s << "  focus       cam" << focused_cam_id_;
        putLine(s.str(), ACCENT_INFO);
    }
    if (show_mask_)  putLine("  view        MASK", ACCENT_WARN);
    if (show_zones_) putLine("  view        +ZONES", ACCENT_INFO);
    if (paused_)    putLine("  state       PAUSED", ACCENT_WARN);
    y += 4;
    putDivider();

    putHeader("ROUND  (" + std::to_string(round_hits_.size()) + " HITS)");
    for (size_t i = 0; i < round_hits_.size(); ++i) {
        const auto& h = round_hits_[i];
        const cv::Scalar col = DART_COLORS[std::min<size_t>(i, 2)];
        std::ostringstream s;
        s << "  #" << (i + 1) << "  " << h.zone
          << "   c=" << std::fixed << std::setprecision(2) << h.confidence
          << "  s=" << std::setprecision(1) << h.sigma_mm << "mm"
          << "  sp=" << pairwiseSpread(h) << "mm";
        putLine(s.str(), col);
    }
    if (round_hits_.empty()) putLine("  (waiting...)", TEXT_FAINT);
    {
        std::ostringstream s;
        s << "  darts  " << darts_in_round_ << " / 3";
        putLine(s.str());
        s.str("");
        s << "  total  " << round_total_score_;
        putLine(s.str());
    }
    y += 4;
    putDivider();

    putHeader("CONTROLS");
    putLine("  click       zoom cam",   TEXT_DIM);
    putLine("  space       play/pause", TEXT_DIM);
    putLine("  </>         step bwd/fwd", TEXT_DIM);
    putLine("  m           mask view",  TEXT_DIM);
    putLine("  z           zones overlay", TEXT_DIM);
    putLine("  1/2/3       focus cam",  TEXT_DIM);
    putLine("  0           grid view",  TEXT_DIM);
    putLine("  r           reset round", TEXT_DIM);
    putLine("  b           refresh bg", TEXT_DIM);
    putLine("  q/ESC       quit",       TEXT_DIM);
}

bool DebugUI::render()
{
    cv::Mat img;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        composite(img);
    }
    cv::imshow(WINDOW_NAME, img);

    const int raw = cv::waitKeyEx(15);
    if (raw < 0) return true;

    const int ascii = raw & 0xff;
    const bool is_left  = (raw == 81)  || (raw == 2424832) || (raw == 63234) || (raw == 65361);
    const bool is_right = (raw == 83)  || (raw == 2555904) || (raw == 63235) || (raw == 65363);

    std::lock_guard<std::mutex> lk(mtx_);
    if (ascii == 'q' || ascii == 27)               return false;
    if (ascii == 'r')                              reset_requested_      = true;
    if (ascii == 'b')                              bg_refresh_requested_ = true;
    if (ascii == ' ')                              paused_               = !paused_;
    if (is_right || ascii == '.' || ascii == 'n')  step_forward_         = true;
    if (is_left  || ascii == ',' || ascii == 'p')  step_backward_        = true;
    if (ascii == 'm')                              show_mask_            = !show_mask_;
    if (ascii == 'z')                              show_zones_           = !show_zones_;
    if (ascii == '0')                              focused_cam_id_       = -1;
    if (ascii == '1' && NUM_CAMS > 0)              focused_cam_id_       = 0;
    if (ascii == '2' && NUM_CAMS > 1)              focused_cam_id_       = 1;
    if (ascii == '3' && NUM_CAMS > 2)              focused_cam_id_       = 2;
    return true;
}

} // namespace camdetect
