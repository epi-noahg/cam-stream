#include "camdetect/Renderer.hpp"

#include <cmath>
#include <opencv2/imgproc.hpp>

namespace camdetect {

namespace {

/// Map board (x,y) in mm to pixel in the canonical image of @p size.
/// Scale so that DOUBLE_OUTER (170 mm) is at 92% of half-size.
cv::Point boardToCanonicalPx(const cv::Point2f& mm, int size)
{
    const float half  = size * 0.5f;
    const float scale = (half * 0.92f) / board::DOUBLE_OUTER;
    // +y in board frame is "up" in image → invert y for image coords
    return {
        cvRound(half + mm.x * scale),
        cvRound(half - mm.y * scale)
    };
}

void drawCircleBoardMM(cv::Mat& img, float r_mm, const cv::Scalar& col,
                       int thickness = 1)
{
    const int size = img.cols;
    const float half  = size * 0.5f;
    const float scale = (half * 0.92f) / board::DOUBLE_OUTER;
    cv::circle(img,
               { cvRound(half), cvRound(half) },
               cvRound(r_mm * scale),
               col, thickness, cv::LINE_AA);
}

} // anon

cv::Mat Renderer::renderCanonicalBoard(int size)
{
    cv::Mat img(size, size, CV_8UC3, cv::Scalar(30, 30, 30));

    // 20 sectors: alternate light/dark grey, with red/green for double/triple.
    const int   cx = size / 2;
    const int   cy = size / 2;
    const float half  = size * 0.5f;
    const float scale = (half * 0.92f) / board::DOUBLE_OUTER;

    const auto fillSector = [&](int s_idx, float r_in, float r_out,
                                const cv::Scalar& color) {
        // angle of sector centre, clockwise from +y
        const float a_centre_deg = s_idx * 18.f;
        const float a_start_deg  = a_centre_deg - 9.f;
        const float a_end_deg    = a_centre_deg + 9.f;
        // OpenCV ellipse "angle" is from +x axis clockwise; convert.
        const float start = 90.f - a_end_deg;
        const float end   = 90.f - a_start_deg;
        std::vector<cv::Point> pts;
        cv::ellipse2Poly({cx, cy},
                          {cvRound(r_out * scale), cvRound(r_out * scale)},
                          0, cvRound(start), cvRound(end), 1, pts);
        std::vector<cv::Point> inner;
        cv::ellipse2Poly({cx, cy},
                          {cvRound(r_in * scale), cvRound(r_in * scale)},
                          0, cvRound(start), cvRound(end), 1, inner);
        for (auto it = inner.rbegin(); it != inner.rend(); ++it)
            pts.push_back(*it);
        const cv::Point* p = pts.data();
        const int        n = static_cast<int>(pts.size());
        cv::fillPoly(img, &p, &n, 1, color);
    };

    for (int s = 0; s < 20; ++s) {
        const bool dark = (s % 2 == 0);
        const cv::Scalar single_col = dark ? cv::Scalar(40, 40, 40)
                                            : cv::Scalar(220, 220, 200);
        // single (inner big ring)
        fillSector(s, board::BULL_RADIUS,   board::TRIPLE_INNER, single_col);
        // triple
        const cv::Scalar trip_col = dark ? cv::Scalar(40, 180, 40)
                                          : cv::Scalar(40, 40, 200);
        fillSector(s, board::TRIPLE_INNER, board::TRIPLE_OUTER, trip_col);
        // outer single
        fillSector(s, board::TRIPLE_OUTER, board::DOUBLE_INNER, single_col);
        // double
        const cv::Scalar doub_col = dark ? cv::Scalar(40, 180, 40)
                                          : cv::Scalar(40, 40, 200);
        fillSector(s, board::DOUBLE_INNER, board::DOUBLE_OUTER, doub_col);
    }

    // Bull + Bullseye
    cv::circle(img, {cx, cy}, cvRound(board::BULL_RADIUS * scale),
               {40, 180, 40}, -1, cv::LINE_AA);
    cv::circle(img, {cx, cy}, cvRound(board::BULLSEYE_RADIUS * scale),
               {40, 40, 200}, -1, cv::LINE_AA);

    // Outline + sector labels
    drawCircleBoardMM(img, board::DOUBLE_OUTER, {255, 255, 255}, 1);
    for (int s = 0; s < 20; ++s) {
        const int val = board::SECTORS[s];
        const float a = s * 18.f * static_cast<float>(M_PI) / 180.f;
        const float r = board::DOUBLE_OUTER + 12.f;
        const cv::Point pos = boardToCanonicalPx(
            {r * std::sin(a), r * std::cos(a)}, size);
        const std::string txt = std::to_string(val);
        int baseline = 0;
        const cv::Size ts = cv::getTextSize(
            txt, cv::FONT_HERSHEY_SIMPLEX, 0.4, 1, &baseline);
        cv::putText(img, txt,
                    {pos.x - ts.width / 2, pos.y + ts.height / 2},
                    cv::FONT_HERSHEY_SIMPLEX, 0.4,
                    {255, 255, 255}, 1, cv::LINE_AA);
    }
    return img;
}

void Renderer::drawHitOnCanonical(cv::Mat&           canonical,
                                   const cv::Point2f& board_xy,
                                   const cv::Scalar&  color,
                                   const std::string& label)
{
    const cv::Point p = boardToCanonicalPx(board_xy, canonical.cols);
    cv::circle(canonical, p, 5, color, -1, cv::LINE_AA);
    cv::circle(canonical, p, 6, {255, 255, 255}, 1, cv::LINE_AA);
    if (!label.empty()) {
        cv::putText(canonical, label, {p.x + 8, p.y - 8},
                    cv::FONT_HERSHEY_SIMPLEX, 0.4,
                    color, 1, cv::LINE_AA);
    }
}

void Renderer::drawCalibrationOverlay(cv::Mat&                frame,
                                       const BoardCalibration& calib)
{
    if (!calib.isValid()) return;

    const auto polyCircle = [&](float r_mm, const cv::Scalar& col) {
        std::vector<cv::Point> pts;
        pts.reserve(96);
        for (int i = 0; i < 96; ++i) {
            const float a = i * (2.f * static_cast<float>(M_PI) / 96.f);
            const cv::Point2f bp{r_mm * std::sin(a), r_mm * std::cos(a)};
            const auto ip = calib.boardToImage(bp);
            pts.emplace_back(cvRound(ip.x), cvRound(ip.y));
        }
        const cv::Point* p = pts.data();
        const int        n = static_cast<int>(pts.size());
        cv::polylines(frame, &p, &n, 1, true, col, 1, cv::LINE_AA);
    };

    polyCircle(board::BULLSEYE_RADIUS, {  0, 255, 255});
    polyCircle(board::BULL_RADIUS,     {  0, 200, 255});
    polyCircle(board::TRIPLE_INNER,    {  0, 255,   0});
    polyCircle(board::TRIPLE_OUTER,    {  0, 255,   0});
    polyCircle(board::DOUBLE_INNER,    {255, 100,   0});
    polyCircle(board::DOUBLE_OUTER,    {255, 100,   0});

    for (int s = 0; s < 20; ++s) {
        const float a = (s * 18.f - 9.f) * static_cast<float>(M_PI) / 180.f;
        const cv::Point2f p_in {
            board::BULL_RADIUS  * std::sin(a),
            board::BULL_RADIUS  * std::cos(a)
        };
        const cv::Point2f p_out{
            board::DOUBLE_OUTER * std::sin(a),
            board::DOUBLE_OUTER * std::cos(a)
        };
        cv::line(frame, calib.boardToImage(p_in), calib.boardToImage(p_out),
                 {180, 180, 180}, 1, cv::LINE_AA);
    }
}

void Renderer::drawDetectionOverlay(cv::Mat&                      frame,
                                     const std::vector<cv::Point>& contour,
                                     const cv::Point2f&            tip_pixel,
                                     const cv::Point2f&            dart_dir)
{
    if (!contour.empty()) {
        std::vector<std::vector<cv::Point>> v{contour};
        cv::drawContours(frame, v, 0, {0, 255, 255}, 2, cv::LINE_AA);
    }

    if (tip_pixel.x > 0 || tip_pixel.y > 0) {
        cv::circle(frame, tip_pixel, 6, {0, 0, 255}, -1, cv::LINE_AA);
        cv::circle(frame, tip_pixel, 8, {255, 255, 255}, 1, cv::LINE_AA);

        if (dart_dir.x != 0.f || dart_dir.y != 0.f) {
            const cv::Point2f tail = tip_pixel - dart_dir * 60.f;
            cv::line(frame, tip_pixel, tail, {0, 255, 255}, 1, cv::LINE_AA);
        }
    }
}

void Renderer::drawFullDart(cv::Mat&           frame,
                             const cv::Point2f& tip_pixel,
                             const cv::Point2f& tail_pixel)
{
    if (tail_pixel.x == 0.f && tail_pixel.y == 0.f) return;
    cv::line(frame, tip_pixel, tail_pixel, {0, 255, 255}, 2, cv::LINE_AA);
    cv::circle(frame, tail_pixel, 4, {255, 200, 0}, -1, cv::LINE_AA);
}

void Renderer::drawLoggedTip(cv::Mat& frame, const cv::Point2f& tip,
                              const cv::Scalar& color)
{
    cv::circle(frame, tip, 5, color, 1, cv::LINE_AA);
    cv::drawMarker(frame, tip, color, cv::MARKER_TILTED_CROSS, 10, 1);
}

} // namespace camdetect
