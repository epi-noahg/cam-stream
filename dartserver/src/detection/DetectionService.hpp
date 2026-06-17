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

    explicit DetectionService(dart::game::GameManager& gm);
    ~DetectionService();

    /// Load calibrations + zone maps and build the Pipeline.  False on error.
    bool init(const Config& cfg);

    /// Start frame feeding (cameras or replay) + the status-polling thread.
    void start();
    void stop();

    void setOnBoardStatus(BoardStatusCallback cb) { on_status_ = std::move(cb); }
    BoardStatus boardStatus() const;

    /// True once a replay run has consumed all frames (never set in live mode).
    bool finished() const { return finished_.load(); }

    // Operator commands forwarded to the pipeline.
    void resetRound();
    void refreshBackground();

private:
    void feedLoopReplay_();
    void feedLoopCamera_(int cam_id);
    void statusLoop_();
    BoardStatus computeStatus_() const;

    dart::game::GameManager&           gm_;
    Config                             cfg_;
    std::unique_ptr<camdetect::Pipeline> pipeline_;

    std::vector<std::thread>           feed_threads_;
    std::thread                        status_thread_;
    std::atomic<bool>                  running_ {false};
    std::atomic<bool>                  finished_ {false};
    BoardStatusCallback                on_status_;

    mutable std::mutex                 status_mtx_;
    BoardStatus                        last_status_;
    // Opaque handles for live capture; defined in the .cpp to avoid leaking
    // CameraCapture (server/src) into this header.
    struct LiveCams;
    std::unique_ptr<LiveCams>          live_;
};

} // namespace dart::detect
