#pragma once

#include "Types.hpp"

#include <array>
#include <optional>

namespace camdetect {

/// Combines per-camera DartHits into one FusedHit.
///
/// A single physical dart makes each camera emit one DartHit, but at slightly
/// different times (inter-camera lag / shutter offset) and slightly different
/// board projections (parallax + calibration error).  This class buffers hits
/// in a time window, clusters them spatially, and confirms a fused hit once at
/// least MIN_CAMS_FOR_CONFIRM cameras agree on a location.
///
/// Timing is driven by tick(now), NOT by hit arrival: an open window still
/// closes when the "missing" camera never reports the dart, as long as some
/// camera keeps feeding frames that advance the clock.  This is what lets a
/// 2-camera hit confirm even though the third camera stayed silent.
///
/// Single-camera votes, and votes that don't spatially agree with any other
/// camera, are dropped as phantoms rather than confirmed.
class MultiCamFusion {
public:
    explicit MultiCamFusion(double window_seconds = 0.5);

    /// Buffer a per-camera hit.  Returns a FusedHit immediately only once every
    /// camera has voted (no reason to wait out the window); otherwise nullopt.
    std::optional<FusedHit> addHit(const DartHit& hit);

    /// Advance the fusion clock.  Closes (and confirms, or drops) an open
    /// window once @p window_seconds have elapsed since the first buffered hit.
    std::optional<FusedHit> tick(double now);

    /// Force-close the current window (shutdown / board-clear).
    std::optional<FusedHit> flush();

    void reset();

    bool hasPending() const { return first_ts_ >= 0.0; }

    /// Minimum number of agreeing cameras required to confirm a dart.
    static constexpr int   MIN_CAMS_FOR_CONFIRM = 2;
    /// Two per-cam projections within this board distance count as agreeing.
    static constexpr float AGREEMENT_RADIUS_MM  = 25.f;

private:
    /// Cluster the buffered votes and build a FusedHit from the largest
    /// agreeing group, or nullopt if fewer than MIN_CAMS_FOR_CONFIRM agree.
    /// Always resets the window.
    std::optional<FusedHit> confirm();

    double                                       window_seconds_;
    std::array<std::optional<DartHit>, NUM_CAMS> pending_{};
    double                                       first_ts_{-1.0};
};

} // namespace camdetect
