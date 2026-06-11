#pragma once

#include "ZoneMapper.hpp"

#include <opencv2/core.hpp>
#include <string>

namespace camdetect {

/// Pixel-accurate scoring map for one camera.
///
/// A label image the exact size of the camera frame where every pixel holds
/// the id of the scoring zone it belongs to.  Built once at startup by the
/// AutoCalibrator (colour segmentation + watershed along the real wires), so
/// zone boundaries follow the actual physical board — including lens
/// distortion — instead of the straight reprojected lines of the homography.
///
/// Id encoding (CV_16U):
///   0           MISS / unknown
///   1           Bullseye (50)
///   2           Outer bull (25)
///   100 + v     Single sector v   (v in 1..20)
///   200 + v     Triple sector v
///   300 + v     Double sector v
class ZoneMap {
public:
    static constexpr uint16_t MISS        = 0;
    static constexpr uint16_t BULL        = 1;
    static constexpr uint16_t OUTER_BULL  = 2;
    static constexpr uint16_t SINGLE_BASE = 100;
    static constexpr uint16_t TRIPLE_BASE = 200;
    static constexpr uint16_t DOUBLE_BASE = 300;

    static ZoneResult idToResult(uint16_t id);

    /// Distinct BGR colour per zone id, for visualisation.
    static cv::Scalar idColor(uint16_t id);

    /// "<dir>/camN.yml" → "<dir>/camN_zones.png" — the conventional location
    /// of the zone map that goes with a calibration file.
    static std::string companionPath(const std::string& calib_yml_path);

    bool     empty() const { return labels_.empty(); }
    cv::Size size()  const { return labels_.size(); }

    const cv::Mat& labels() const { return labels_; }
    void setLabels(cv::Mat labels);   // must be CV_16U

    /// Zone at an image pixel.  Out-of-bounds → MISS.
    ZoneResult lookup(const cv::Point2f& px) const;

    /// Distance (px) from @p px to the nearest pixel with a DIFFERENT zone
    /// id — i.e. how far the tip sits from the closest wire, as seen by this
    /// camera, lens distortion included.  Capped at @p max_search_px (also
    /// returned when the map is empty or @p px is out of bounds).
    float boundaryDistancePx(const cv::Point2f& px,
                             float max_search_px = 40.f) const;

    bool saveToFile(const std::string& png_path) const;
    bool loadFromFile(const std::string& png_path);

    /// Colourised blend over @p frame for visual verification.  When
    /// @p annotate is true the zone labels ("20", "T20", …) are drawn at
    /// each zone's centroid.
    cv::Mat overlay(const cv::Mat& frame, float alpha = 0.45f,
                    bool annotate = true) const;

private:
    cv::Mat labels_;   // CV_16U
};

} // namespace camdetect
