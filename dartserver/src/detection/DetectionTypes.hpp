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

/// One camera's calibration files + geometry, surfaced to the calibration UI.
struct CalibCamInfo {
    int         camId      {0};
    std::string calibPath;         ///< camN.yml
    std::string zonesPath;         ///< camN_zones.png
    bool        hasCalib   {false};///< calib yml present + loaded
    bool        hasZones   {false};///< companion zone map present
    int         width      {0};
    int         height     {0};
    float       orientationDeg {0.f};
    float       diffThreshold   {-1.f};
};

/// AutoCalibrator knobs the UI can tweak before a scan.
struct AutoCalibOptions {
    int   redADelta     {16};
    int   greenADelta   {12};
    int   minChroma     {16};
    int   sector20Offset{0};
    float sector20HintX {-1.f};   ///< sector-20 click, normalized [0,1] (-1 = unset)
    float sector20HintY {-1.f};   ///< converted to pixels against the actual frame
    bool  autotune      {false};  ///< sweep colour thresholds before running
};

/// Outcome of one auto-scan: diagnostics + a base64 JPEG overlay for preview.
/// Held pending in DetectionService until the operator saves it.
struct AutoCalibOutcome {
    bool        ok        {false};
    std::string error;
    std::string warning;
    int         triplesFound {0};
    int         doublesFound {0};
    float       meanReprojErrPx {-1.f};
    // Effective colour thresholds used (echoes autotune results back to the UI).
    int         redADelta  {16};
    int         greenADelta{12};
    int         minChroma  {16};
    std::string overlayBase64;    ///< JPEG bytes, base64, no data: prefix
};

} // namespace dart::detect
