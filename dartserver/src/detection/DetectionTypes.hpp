#pragma once

// Board / camera status surfaced to the UI so it knows when the cameras are
// ready to score and what the dartboard is doing.  Decoupled from camdetect's
// internal enums so the API layer never includes OpenCV headers.

#include <array>
#include <string>

namespace dart::detect {

constexpr int NUM_CAMS = 3;

/// Readiness of a single camera's detector.
enum class CamReady {
    Warmup,   ///< still learning the background
    Normal,   ///< ready / tracking
    Human,    ///< a hand/arm is in frame, detection paused
    Clean     ///< board empty, ready for the next round
};

const char* toString(CamReady s);

struct CamStatus {
    int      id    {0};
    CamReady state {CamReady::Warmup};
    bool     ready {false};   ///< Normal or Clean
};

struct RoundInfo {
    std::string phase;        ///< "waiting" | "complete" | "resyncing"
    int         nextDart {1}; ///< 1..3 when waiting
    std::string message;      ///< short human-readable label
};

/// Aggregate board status broadcast on every change.
struct BoardStatus {
    std::array<CamStatus, NUM_CAMS> cams {};
    RoundInfo                       round;
    bool                            allReady {false};  ///< every cam ready

    bool operator==(const BoardStatus& o) const;
    bool operator!=(const BoardStatus& o) const { return !(*this == o); }
};

} // namespace dart::detect
