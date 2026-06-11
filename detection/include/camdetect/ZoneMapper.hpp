#pragma once

#include <opencv2/core.hpp>
#include <string>

namespace camdetect {

struct ZoneResult {
    std::string label;   // "T20", "D5", "20", "Bull", "25", "MISS"
    int         value;
};

/// Pure geometry: board-space (x,y) in millimetres → scoring zone.
class ZoneMapper {
public:
    static ZoneResult lookup(const cv::Point2f& board_xy);

    /// Distance (mm) from @p board_xy to the nearest scoring boundary — ring
    /// edge or sector wire.  The zone label only survives a positional error
    /// smaller than this margin, so margin/sigma is exactly how safe a hit's
    /// label is.
    static float boundaryMarginMM(const cv::Point2f& board_xy);
};

} // namespace camdetect
