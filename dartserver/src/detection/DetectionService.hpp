#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// DetectionService — owns the camdetect Pipeline and bridges vision → game.
//
//   • Feeds frames to the Pipeline, from live V4L2 cameras (CameraCapture) or
//     from recorded videos (FileSource, "replay" mode for hardware-free tests).
//   • Translates each FusedHit (zone string + score + confidence) into a
//     game Throw and forwards it to the GameManager, flagging low-confidence
//     darts as needing review.
//   • Polls the per-camera detector states + round phase and publishes a
//     BoardStatus so the UI knows when the board is ready to score.
//
// This is the only place that includes camdetect/OpenCV; everything downstream
// works with plain game/detection types.
// ─────────────────────────────────────────────────────────────────────────────

#include "DetectionTypes.hpp"
#include "game/GameManager.hpp"
#include "game/GameTypes.hpp"

#include <opencv2/core.hpp>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace camdetect { class Pipeline; }

namespace dart::detect {

/// Parse a detector zone label ("T20","D5","20","Bull","25","MISS") into a
/// game Throw.  Exposed for unit testing.
dart::game::Throw zoneToThrow(const std::string& zone);

class DetectionService {
public:
    struct Config {
        std::array<std::string, NUM_CAMS> calibPaths {};   ///< camN.yml
        // Replay mode: feed recorded videos instead of live cameras.
        bool                              replay {false};
        std::array<std::string, NUM_CAMS> videoPaths {};
        std::array<int, NUM_CAMS>         offsets {{0, 0, 0}}; ///< per-cam frame offsets
        bool                              loop {false};       ///< restart replay at EOF
        bool                              realtime {true};    ///< pace replay at video fps
        // Live mode (V4L2):
        std::array<std::string, NUM_CAMS> devices {
            {"/dev/video0", "/dev/video1", "/dev/video2"}};
        int width  {640};
        int height {480};
        int fps    {30};
        // A detection below this confidence is flagged needsReview.
        float reviewThreshold {0.55f};
    };

    using BoardStatusCallback = std::function<void(const BoardStatus&)>;
    /// Fired when an async auto-scan finishes; carries the camera + outcome.
    using AutoCalibCallback   = std::function<void(int, const AutoCalibOutcome&)>;

    explicit DetectionService(dart::game::GameManager& gm);
    ~DetectionService();

    /// Load calibrations + zone maps and build the Pipeline.  False on error.
    bool init(const Config& cfg);

    /// Start frame feeding (cameras or replay) + the status-polling thread.
    void start();
    void stop();

    void setOnBoardStatus(BoardStatusCallback cb) { on_status_ = std::move(cb); }
    void setOnAutoCalib(AutoCalibCallback cb) { on_autocalib_ = std::move(cb); }
    BoardStatus boardStatus() const;

    /// True once a replay run has consumed all frames (never set in live mode).
    bool finished() const { return finished_.load(); }

    // Operator commands forwarded to the pipeline.
    void resetRound();
    void refreshBackground();

    // ── Calibration (run from the app's calibration tab) ─────────────────
    /// True in --replay mode (cameras are recorded videos, not live V4L2).
    bool isReplay() const { return cfg_.replay; }
    /// Per-camera calibration files + geometry, read fresh from disk.
    std::vector<CalibCamInfo> calibrationInfo() const;
    /// Latest frame from one camera as a base64 JPEG (scaled to maxWidth),
    /// or "" if no frame is available yet.
    std::string cameraSnapshotJpeg(int cam, int maxWidth = 480,
                                   bool overlayCurrent = false) const;
    /// Run the AutoCalibrator on the latest frame of one camera.  The result
    /// (calibration + zone map + the frame) is held pending until
    /// saveCalibration() persists it; the returned outcome carries the
    /// diagnostics + an overlay preview for the UI.
    AutoCalibOutcome runAutoCalib(int cam, const AutoCalibOptions& opt);
    /// Non-blocking variant: runs the scan on a worker thread and delivers the
    /// outcome via the AutoCalibCallback, so the WS connection keeps serving
    /// snapshots/commands while a (possibly slow) scan runs.  Returns false if
    /// a scan is already in flight.
    bool runAutoCalibAsync(int cam, const AutoCalibOptions& opt);
    /// Persist the pending auto-calib result (camN.yml + camN_zones.png) and
    /// hot-swap it into the live pipeline.  False (+ err) if nothing pending.
    bool saveCalibration(int cam, std::string& err);

    // ── Replay transport (only meaningful in --replay mode) ──────────────
    // Drive playback from a local control window on the Mac.
    void replayTogglePause();
    void replayStep(int frames = 1);   ///< feed N frames then pause
    void replaySeek(int frameTick);    ///< jump to an absolute position
    bool replayPaused() const { return paused_.load(); }
    int  replayPos() const    { return replay_pos_.load(); }
    int  replayTotal() const  { return replay_total_.load(); }
    /// Compose the latest fed frames into one image (scaled to maxWidth).
    /// False if no frame is available yet.
    bool replaySnapshot(cv::Mat& out, int maxWidth) const;

private:
    void feedLoopReplay_();
    void feedLoopCamera_(int cam_id);
    void processLoopCamera_(int cam_id);
    void statusLoop_();
    BoardStatus computeStatus_() const;
    /// Copy the latest frame for one camera out of display_frames_.
    bool latestFrame_(int cam, cv::Mat& out) const;

    dart::game::GameManager&           gm_;
    Config                             cfg_;
    std::unique_ptr<camdetect::Pipeline> pipeline_;

    std::vector<std::thread>           feed_threads_;
    std::vector<std::thread>           proc_threads_;
    std::thread                        status_thread_;
    std::atomic<bool>                  running_ {false};
    std::atomic<bool>                  finished_ {false};
    BoardStatusCallback                on_status_;
    AutoCalibCallback                  on_autocalib_;
    std::atomic<bool>                  scanning_ {false};
    std::thread                        scan_thread_;

    // Replay transport state (driven by the control window).
    std::atomic<bool>                  paused_ {false};
    std::atomic<int>                   step_ {0};
    std::atomic<int>                   seek_target_ {-1};
    std::atomic<int>                   replay_pos_ {0};
    std::atomic<int>                   replay_total_ {0};
    mutable std::mutex                 display_mtx_;
    std::array<cv::Mat, NUM_CAMS>      display_frames_;

    mutable std::mutex                 status_mtx_;
    BoardStatus                        last_status_;
    // Opaque handles for live capture; defined in the .cpp to avoid leaking
    // CameraCapture (server/src) into this header.
    struct LiveCams;
    std::unique_ptr<LiveCams>          live_;

    // Per-camera capture→processing handoff (latest-frame-wins).  The capture
    // callback only stores the newest frame + notifies; a dedicated worker
    // thread (processLoopCamera_) processes the latest frame and drops any
    // skipped ones, so the camera is never throttled by processFrame cost.
    // Opaque (holds mutex/condvar/cv::Mat) → defined in the .cpp.
    struct CamHandoff;
    std::unique_ptr<CamHandoff>        handoff_;

    // Pending auto-calib results awaiting save (one per camera).  Holds
    // camdetect types (BoardCalibration / ZoneMap), so it is defined in the
    // .cpp to keep camdetect/OpenCV out of this header.
    struct PendingCalibs;
    std::unique_ptr<PendingCalibs>     pending_;
    mutable std::mutex                 pending_mtx_;
};

} // namespace dart::detect
