#!/usr/bin/env bash
# Build the standalone camdetect project (detection/) — the camdetect static
# library plus the calibration/debug tools (camdetect_autocalib, _calibrate,
# _offline, _debug, _annotate, _runtest, _live).
#
#   scripts/build-detection.sh [dev|release]   (default: release)
#
#   dev      → CMAKE_BUILD_TYPE=Debug   in detection/build-debug
#   release  → CMAKE_BUILD_TYPE=Release in detection/build
#
# Note: the dartserver build already compiles camdetect into the server binary
# via add_subdirectory(../detection). This script is only needed when you also
# want the standalone command-line tools rebuilt (e.g. recalibration on the box).
#
# Ubuntu deps: same as the server (build-essential cmake libopencv-dev …).

source "$(dirname "$0")/lib/common.sh"

PROFILE="$(parse_profile "${1:-release}")"
[ "$PROFILE" = "dev" ] && BUILD_TYPE="Debug" || BUILD_TYPE="Release"
[ "$PROFILE" = "dev" ] && BUILD_DIR="$DETECTION_SRC/build-debug" || BUILD_DIR="$DETECTION_SRC/build"

require_cmd cmake "Install with: sudo apt install -y cmake build-essential"
require_cmd c++   "Install with: sudo apt install -y build-essential"

info "Configuring camdetect ($BUILD_TYPE) → $BUILD_DIR"
run cmake -S "$DETECTION_SRC" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

info "Building (-j$(nproc_safe))"
run cmake --build "$BUILD_DIR" -j"$(nproc_safe)"

ok "Detection tools built: $BUILD_DIR/"
