#pragma once

#include "BoardCalibration.hpp"
#include "ZoneMap.hpp"

#include <opencv2/core.hpp>
#include <string>

namespace camdetect {

/// One-shot automatic board calibration from a single clean frame.
///
/// Pipeline:
///   1. LAB colour segmentation → red / green blobs (doubles, triples,
///      bull, bullseye) — these ARE the pixel-accurate ring zones.
///   2. Ellipse fit on the blob cloud → normalised board space; blobs are
///      binned into the triple / double bands and into 20 angular slots.
///   3. Orientation: the slot nearest "image up" is sector 20, corrected by
///      ring-colour parity (20 is red).  Overridable: a user click anywhere
///      inside sector 20, and/or a manual rotation offset.
///   4. RANSAC homography from blob centroids ↔ known board positions
///      (also produces a BoardCalibration that drops into the old system).
///   5. Watershed seeded by the ring blobs + homography-projected single
///      seeds → every pixel gets a zone id, with boundaries snapped to the
///      real wires (no straight-line assumption, fisheye-friendly).
class AutoCalibrator {
public:
    struct Options {
        // Colour segmentation (LAB, neutral a* = 128).
        int red_a_delta    {16};  ///< a* above neutral to call a pixel red
        int green_a_delta  {12};  ///< a* below neutral to call a pixel green
        int min_chroma     {16};  ///< reject near-grey pixels
        int min_blob_area  {15};  ///< px², drops segmentation speckle

        /// Manual rotation of the sector assignment, in sectors clockwise.
        /// Applied after auto orientation; lets the user nudge with keys.
        int sector20_offset {0};

        /// Optional pixel the user clicked anywhere inside sector 20.
        /// (-1,-1) = unset, use auto orientation.
        cv::Point2f sector20_hint {-1.f, -1.f};

    };

    struct Result {
        bool             ok {false};
        std::string      error;          ///< set when !ok
        std::string      warning;        ///< non-fatal findings, "" if clean

        BoardCalibration calibration;    ///< drop-in for the legacy system
        ZoneMap          zone_map;       ///< the pixel-accurate scoring map

        // Diagnostics for the UI / logs.
        cv::Mat red_mask, green_mask;    ///< CV_8U cleaned colour masks
        int     triples_found {0};       ///< ring blobs matched (ideal: 20)
        int     doubles_found {0};
        float   mean_reproj_err_px {-1.f};
    };

    Result run(const cv::Mat& frame_bgr, const Options& opt) const;
    Result run(const cv::Mat& frame_bgr) const { return run(frame_bgr, Options{}); }

    /// Sweep colour-threshold parameters to maximise ring-segment count and
    /// minimise reprojection error.  Returns the best Options found.
    /// Keeps sector20_hint / sector20_offset / use_ocr_orientation unchanged.
    Options tune(const cv::Mat& frame_bgr, const Options& base) const;
    Options tune(const cv::Mat& frame_bgr) const { return tune(frame_bgr, Options{}); }
};

} // namespace camdetect
