#include "camdetect/ZoneMapper.hpp"
#include "camdetect/Types.hpp"

#include <cmath>

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

} // namespace camdetect
