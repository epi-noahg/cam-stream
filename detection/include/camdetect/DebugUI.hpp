#pragma once

#include "BoardCalibration.hpp"
#include "Types.hpp"
#include "ZoneMap.hpp"

#include <array>
#include <mutex>
#include <opencv2/core.hpp>
#include <string>
#include <utility>
#include <vector>

namespace camdetect {

/// Single OpenCV window that composites 3 cam frames, a canonical board view
/// and a text info panel.  All updates are thread-safe; render() must be
/// called from the main thread (macOS requirement).
class DebugUI {
public:
    static constexpr const char* WINDOW_NAME = "camdetect debug";

    DebugUI(std::array<BoardCalibration, NUM_CAMS> calibs,
            int cam_tile_width  = 640,
            int cam_tile_height = 480,
            int board_view_size = 480);

    ~DebugUI();

    DebugUI(const DebugUI&)            = delete;
    DebugUI& operator=(const DebugUI&) = delete;

    /// Push the latest frame + per-cam visualisation for one camera.
    void updateCam(int cam_id, const cv::Mat& frame, const DetectorViz& viz);

    /// Register a pixel-accurate zone map for one camera; enables the 'z'
    /// overlay toggle on that cam's tile.  The colour/edge layers are
    /// precomputed here so per-frame rendering stays a cheap blend.
    void setZoneMap(int cam_id, const ZoneMap& zm);

    /// All fused hits in the current round (up to 3, throw order).
    /// The hit with highest confidence is auto-highlighted.
    void setRoundHits(std::vector<FusedHit> hits);

    /// Update displayed round counters.
    void setRoundProgress(int darts_in_round, int round_total_score);

    /// Update the threshold readout (the trackbar manages its own state).
    void setDiffThreshold(float v);

    /// Update the per-cam delay readout (in frames, can be negative).
    void setCamDelay(int cam_id, int frames);

    /// Set the high-level round phase + message shown to the player.
    void setRoundStatus(const std::string& message, int phase_index);

    /// Extra host-tool section in the info panel (e.g. annotation state).
    /// Hidden while @p lines is empty.
    void setExtraPanel(std::string header, std::vector<std::string> lines);

    /// Render one frame.  @returns false if the user requested to quit.
    bool render();

    bool consumeResetRequest();
    bool consumeBgRefreshRequest();
    /// True once per press of '.' / 'n' / RIGHT arrow.
    bool consumeStepForward();
    /// True once per press of ',' / 'p' / LEFT arrow.
    bool consumeStepBackward();
    /// Last ascii key pressed in render() (0 if none since the previous
    /// call).  Lets host tools layer extra shortcuts on top of the built-ins.
    int  consumeKey();
    bool isPaused() const;

    /// Currently focused cam (-1 = grid view, 0..N-1 = zoomed on that cam).
    int  focusedCam() const;

    /// Mouse hit handler (public so the static GUI callback can forward to it).
    void handleClick(int x, int y);

private:
    void composite(cv::Mat& out) const;
    void drawCamTile(cv::Mat& out, const cv::Rect& roi, int cam_id,
                     bool draw_label = true, float font_scale = 0.55f) const;

    std::array<BoardCalibration, NUM_CAMS> calibs_;
    int tile_w_;
    int tile_h_;
    int board_size_;

    mutable std::mutex                       mtx_;
    std::array<cv::Mat,      NUM_CAMS>       frames_;
    std::array<DetectorViz,  NUM_CAMS>       vizs_;
    std::vector<FusedHit>                    round_hits_;
    int                                      darts_in_round_   {0};
    int                                      round_total_score_{0};
    float                                    diff_threshold_   {15.f};
    std::array<int, NUM_CAMS>                cam_delays_       {{0, 0, 0}};
    std::string                              round_status_msg_;
    int                                      round_phase_      {0};
    std::string                              extra_header_;
    std::vector<std::string>                 extra_lines_;
    int                                      last_key_         {0};

    // Precomputed zone-overlay layers (from setZoneMap).
    std::array<cv::Mat, NUM_CAMS>            zone_color_;   // CV_8UC3 tint
    std::array<cv::Mat, NUM_CAMS>            zone_area_;    // CV_8U, non-MISS
    std::array<cv::Mat, NUM_CAMS>            zone_edges_;   // CV_8U, id changes

    // UI state
    bool reset_requested_     {false};
    bool bg_refresh_requested_{false};
    bool step_forward_        {false};
    bool step_backward_       {false};
    bool paused_              {false};
    bool show_mask_           {false};
    bool show_zones_          {false};
    int  focused_cam_id_      {-1};

    // ROIs of the last composite, for mouse hit-testing.  composite() is
    // logically const → mutable.
    mutable std::array<cv::Rect, NUM_CAMS> last_tile_rois_ {};
};

} // namespace camdetect
