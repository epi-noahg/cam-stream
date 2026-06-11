#include "camdetect/ZoneMap.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace camdetect {

ZoneResult ZoneMap::idToResult(uint16_t id)
{
    if (id == BULL)       return {"Bull", 50};
    if (id == OUTER_BULL) return {"25",   25};

    const auto sector = [](uint16_t v) { return v >= 1 && v <= 20; };
    if (id > DOUBLE_BASE && sector(id - DOUBLE_BASE)) {
        const int v = id - DOUBLE_BASE;
        return {"D" + std::to_string(v), v * 2};
    }
    if (id > TRIPLE_BASE && sector(id - TRIPLE_BASE)) {
        const int v = id - TRIPLE_BASE;
        return {"T" + std::to_string(v), v * 3};
    }
    if (id > SINGLE_BASE && sector(id - SINGLE_BASE)) {
        const int v = id - SINGLE_BASE;
        return {std::to_string(v), v};
    }
    return {"MISS", 0};
}

cv::Scalar ZoneMap::idColor(uint16_t id)
{
    if (id == MISS)       return {40,  40,  40};
    if (id == BULL)       return {0,   0,   220};
    if (id == OUTER_BULL) return {0,   180, 0};

    const int v    = id % 100;
    const int ring = id / 100;        // 1 single, 2 triple, 3 double
    // Hue from the sector value so adjacent sectors (different values)
    // always get clearly different colours; ring sets the "family".
    const int hue = (v * 53) % 180;
    cv::Mat hsv(1, 1, CV_8UC3,
                cv::Scalar(hue,
                           ring == 1 ? 120 : 255,
                           ring == 1 ? 160 : 230));
    cv::Mat bgr;
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
    const auto px = bgr.at<cv::Vec3b>(0, 0);
    return {double(px[0]), double(px[1]), double(px[2])};
}

std::string ZoneMap::companionPath(const std::string& calib_yml_path)
{
    std::string stem = calib_yml_path;
    for (const char* ext : {".yml", ".yaml"}) {
        const size_t n = std::string(ext).size();
        if (stem.size() > n && stem.compare(stem.size() - n, n, ext) == 0) {
            stem.resize(stem.size() - n);
            break;
        }
    }
    return stem + "_zones.png";
}

void ZoneMap::setLabels(cv::Mat labels)
{
    CV_Assert(labels.empty() || labels.type() == CV_16U);
    labels_ = std::move(labels);
}

ZoneResult ZoneMap::lookup(const cv::Point2f& px) const
{
    const int x = cvRound(px.x);
    const int y = cvRound(px.y);
    if (labels_.empty() ||
        x < 0 || y < 0 || x >= labels_.cols || y >= labels_.rows)
        return {"MISS", 0};
    return idToResult(labels_.at<uint16_t>(y, x));
}

float ZoneMap::boundaryDistancePx(const cv::Point2f& px,
                                  float max_search_px) const
{
    const int cx = cvRound(px.x);
    const int cy = cvRound(px.y);
    if (labels_.empty() ||
        cx < 0 || cy < 0 || cx >= labels_.cols || cy >= labels_.rows)
        return max_search_px;

    const uint16_t center = labels_.at<uint16_t>(cy, cx);
    const int      R      = static_cast<int>(std::ceil(max_search_px));
    const int x0 = std::max(0, cx - R), x1 = std::min(labels_.cols - 1, cx + R);
    const int y0 = std::max(0, cy - R), y1 = std::min(labels_.rows - 1, cy + R);

    // One dart per call — a brute-force window scan (~(2R)² label reads) is
    // plenty fast and has no failure modes.
    float best2 = max_search_px * max_search_px;
    for (int y = y0; y <= y1; ++y) {
        const auto* row = labels_.ptr<uint16_t>(y);
        const float dy  = static_cast<float>(y - cy);
        for (int x = x0; x <= x1; ++x) {
            if (row[x] == center) continue;
            const float dx = static_cast<float>(x - cx);
            const float d2 = dx * dx + dy * dy;
            if (d2 < best2) best2 = d2;
        }
    }
    return std::sqrt(best2);
}

bool ZoneMap::saveToFile(const std::string& png_path) const
{
    if (labels_.empty()) return false;
    return cv::imwrite(png_path, labels_);
}

bool ZoneMap::loadFromFile(const std::string& png_path)
{
    // Probe first: imread logs a warning on missing files, and "no zone map
    // yet" is a perfectly normal situation.
    std::ifstream probe(png_path);
    if (!probe.good()) return false;
    cv::Mat m = cv::imread(png_path, cv::IMREAD_UNCHANGED);
    if (m.empty() || m.type() != CV_16U) return false;
    labels_ = m;
    return true;
}

cv::Mat ZoneMap::overlay(const cv::Mat& frame, float alpha, bool annotate) const
{
    cv::Mat out = frame.clone();
    if (labels_.empty() || frame.size() != labels_.size()) return out;

    // Colour layer + per-zone centroid accumulation in one pass.
    cv::Mat color(frame.size(), CV_8UC3, cv::Scalar(0, 0, 0));
    struct Acc { double x{0}, y{0}; int n{0}; };
    std::map<uint16_t, Acc> acc;
    std::map<uint16_t, cv::Vec3b> palette;

    for (int y = 0; y < labels_.rows; ++y) {
        const auto* lr = labels_.ptr<uint16_t>(y);
        auto*       cr = color.ptr<cv::Vec3b>(y);
        for (int x = 0; x < labels_.cols; ++x) {
            const uint16_t id = lr[x];
            if (id == MISS) continue;
            auto it = palette.find(id);
            if (it == palette.end()) {
                const cv::Scalar c = idColor(id);
                it = palette.emplace(id, cv::Vec3b(uchar(c[0]), uchar(c[1]),
                                                   uchar(c[2]))).first;
            }
            cr[x] = it->second;
            auto& a = acc[id];
            a.x += x; a.y += y; ++a.n;
        }
    }

    cv::addWeighted(color, alpha, out, 1.0, 0.0, out);

    // White hairline where the zone id changes — makes the cut visible.
    cv::Mat edges, gx, gy;
    cv::Sobel(labels_, gx, CV_32F, 1, 0, 1);
    cv::Sobel(labels_, gy, CV_32F, 0, 1, 1);
    edges = (cv::abs(gx) + cv::abs(gy)) > 0;
    out.setTo(cv::Scalar(255, 255, 255), edges);

    if (annotate) {
        for (const auto& [id, a] : acc) {
            if (a.n < 30) continue;
            // Singles get the bare number; rings keep their T/D prefix.
            const std::string txt = idToResult(id).label;
            const cv::Point p(int(a.x / a.n) - 8, int(a.y / a.n) + 4);
            cv::putText(out, txt, p, cv::FONT_HERSHEY_SIMPLEX, 0.38,
                        {0, 0, 0}, 2, cv::LINE_AA);
            cv::putText(out, txt, p, cv::FONT_HERSHEY_SIMPLEX, 0.38,
                        {255, 255, 255}, 1, cv::LINE_AA);
        }
    }
    return out;
}

} // namespace camdetect
