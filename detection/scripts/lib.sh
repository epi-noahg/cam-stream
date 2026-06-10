# Shared configuration + helpers for the camdetect run scripts.
# Sourced by setup_cams.sh / debug_live.sh / debug_videos.sh — not run directly.
#
# Override any of these by exporting them before calling a script, e.g.
#   CALIB_DIR=~/myrig VIDEO_DIR=~/clips ./debug_videos.sh

# Resolve paths relative to this file so the scripts work from any CWD.
LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DETECTION_DIR="$(cd "$LIB_DIR/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-$DETECTION_DIR/build}"
CALIB_DIR="${CALIB_DIR:-$DETECTION_DIR}"            # camN.yml (+ camN_zones.png)
VIDEO_DIR="${VIDEO_DIR:-$DETECTION_DIR/../client}"  # recorded camN_*.mp4

NUM_CAMS="${NUM_CAMS:-3}"

die() { echo "error: $*" >&2; exit 1; }

# Absolute path to a built tool, validated.  Prints the path on stdout.
binary() {
    local bin="$BUILD_DIR/$1"
    [ -x "$bin" ] || die "$1 not built — run:  cmake --build '$BUILD_DIR' -j"
    printf '%s\n' "$bin"
}

# Calibration yml for camera $1.
cam_yml() { printf '%s\n' "$CALIB_DIR/cam$1.yml"; }

# Newest recording for camera $1 (camN_*.mp4, else a plain camN.mp4).
cam_video() {
    local v
    v="$(ls -t "$VIDEO_DIR"/cam"$1"_*.mp4 2>/dev/null | head -n1)"
    [ -n "$v" ] || v="$VIDEO_DIR/cam$1.mp4"
    printf '%s\n' "$v"
}

# Iteration helper: prints "0 1 2 ..." up to NUM_CAMS-1.
cam_indices() { local i=0; while [ "$i" -lt "$NUM_CAMS" ]; do printf '%s ' "$i"; i=$((i+1)); done; }
