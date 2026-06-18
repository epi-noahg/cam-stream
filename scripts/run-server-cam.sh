#!/usr/bin/env bash
# Run the dart server against live cameras (--live). WebSocket API on :8080.
#
#   scripts/run-server-cam.sh [extra dartserver flags...]
#
# Env:
#   BUILD            which build to run: dev | release (default: auto-detect)
#   CALIB0/1/2       calibration files (default: detection/cam{0,1,2}.yml)
#   DEVICES          optional camera devices, e.g. "/dev/video0 /dev/video1 /dev/video2"
#
# Examples:
#   scripts/run-server-cam.sh
#   DEVICES="/dev/video0 /dev/video1 /dev/video2" scripts/run-server-cam.sh

source "$(dirname "$0")/lib/common.sh"

BIN="$(resolve_server_bin)"
default_calib

DEV_ARGS=()
if [ -n "${DEVICES:-}" ]; then
  # shellcheck disable=SC2206
  DEV_ARGS=($DEVICES)
  [ "${#DEV_ARGS[@]}" -eq 3 ] || die "DEVICES must list exactly 3 devices (got ${#DEV_ARGS[@]})"
fi

info "Running: $(basename "$BIN") --live (cameras)"
cd "$REPO_ROOT"   # so dartserver.db lands at the repo root, like dev runs
exec "$BIN" --live ${DEV_ARGS[@]+"${DEV_ARGS[@]}"} "$CALIB0" "$CALIB1" "$CALIB2" "$@"
