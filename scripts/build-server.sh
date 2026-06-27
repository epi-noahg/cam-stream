#!/usr/bin/env bash
# Build the authoritative C++ dart server (dartserver).
#
#   scripts/build-server.sh [dev|release]   (default: release)
#
#   dev      → CMAKE_BUILD_TYPE=Debug   in dartserver/build-debug
#   release  → CMAKE_BUILD_TYPE=Release in dartserver/build
#
# Ubuntu deps:
#   sudo apt install -y build-essential cmake pkg-config \
#       libopencv-dev libboost-all-dev libsqlite3-dev nlohmann-json3-dev

source "$(dirname "$0")/lib/common.sh"

PROFILE="$(parse_profile "${1:-release}")"
[ "$PROFILE" = "dev" ] && BUILD_TYPE="Debug" || BUILD_TYPE="Release"
BUILD_DIR="$(server_build_dir "$PROFILE")"

require_cmd cmake "Install with: sudo apt install -y cmake build-essential"
require_cmd c++   "Install with: sudo apt install -y build-essential"

info "Configuring dartserver ($BUILD_TYPE) → $BUILD_DIR"
run cmake -S "$SERVER_SRC" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

info "Building (-j$(nproc_safe))"
run cmake --build "$BUILD_DIR" -j"$(nproc_safe)"

if [ "$PROFILE" = "dev" ]; then
  info "Running unit tests (dev build)"
  run ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

ok "Server built: $BUILD_DIR/dartserver"
