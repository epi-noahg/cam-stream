#include "camdetect/ZoneMapper.hpp"
#include "camdetect/Types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace camdetect {

ZoneResult ZoneMapper::lookup(const cv::Point2f& p)
{
    const float r = std::sqrt(p.x * p.x + p.y * p.y);

    if (r <= board::BULLSEYE_RADIUS) return {"Bull", 50};
    if (r <= board::BULL_RADIUS)     return {"25",   25};
    if (r >  board::DOUBLE_OUTER)    return {"MISS",  0};

    // Sector index: 0 = top (+y), increasing clockwise.
    //   atan2 gives angle from +x axis counterclockwise; convert to
    //   "clockwise from +y" = (90° - atan2) mod 360°.
    const float deg = std::fmod(
        90.f - std::atan2(p.y, p.x) * 180.f / static_cast<float>(M_PI) + 360.f,
        360.f);
    const int sector_idx = static_cast<int>(std::floor((deg + 9.f) / 18.f)) % 20;
    const int sector_val = board::SECTORS[sector_idx];

    const bool is_triple = (r >= board::TRIPLE_INNER && r <= board::TRIPLE_OUTER);
    const bool is_double = (r >= board::DOUBLE_INNER && r <= board::DOUBLE_OUTER);

    if (is_triple) return {"T" + std::to_string(sector_val), sector_val * 3};
    if (is_double) return {"D" + std::to_string(sector_val), sector_val * 2};
    return {std::to_string(sector_val), sector_val};
}

float ZoneMapper::boundaryMarginMM(const cv::Point2f& p)
{
    const float r = std::sqrt(p.x * p.x + p.y * p.y);

    // Nearest ring boundary (radial distance).
    constexpr std::array<float, 6> RINGS = {
        board::BULLSEYE_RADIUS, board::BULL_RADIUS,
        board::TRIPLE_INNER,    board::TRIPLE_OUTER,
        board::DOUBLE_INNER,    board::DOUBLE_OUTER};
    float margin = std::numeric_limits<float>::max();
    for (const float rb : RINGS)
        margin = std::min(margin, std::abs(r - rb));

    // Nearest sector wire — only exists between the outer bull and the board
    // edge.  Wires sit at 9° + k·18° from +y; perpendicular distance to the
    // closest one is r·sin(Δθ).
    if (r > board::BULL_RADIUS && r <= board::DOUBLE_OUTER) {
        const float deg = std::fmod(
            90.f - std::atan2(p.y, p.x) * 180.f / static_cast<float>(M_PI)
                + 360.f,
            360.f);
        const float into_sector = std::fmod(deg + 9.f, 18.f);   // 0..18
        const float dtheta_deg  = std::min(into_sector, 18.f - into_sector);
        const float wire_dist   =
            r * std::sin(dtheta_deg * static_cast<float>(M_PI) / 180.f);
        margin = std::min(margin, wire_dist);
    }
    return margin;
}

} // namespace camdetect
