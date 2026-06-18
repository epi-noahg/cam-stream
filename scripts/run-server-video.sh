#!/usr/bin/env bash
# Run the dart server against recorded videos (--replay) — no cameras needed.
# WebSocket API on :8080, same as the live server.
#
#   scripts/run-server-video.sh [extra dartserver flags...]
#
# Env:
#   BUILD            which build to run: dev | release (default: auto-detect)
#   CALIB0/1/2       calibration files (default: detection/cam{0,1,2}.yml)
#   VIDEO0/1/2       recordings (default: client/cam{0,1,2}_*.mp4)
#   LOOP             1 → loop the videos (adds --loop)
#   OFFSETS          per-video start frame offsets, e.g. "0,5,2" (adds --offsets)
#   WINDOW           1 → local pause/play/scrub window (needs a display; not on a headless server)

source "$(dirname "$0")/lib/common.sh"

BIN="$(resolve_server_bin)"
default_calib
default_videos

FLAGS=()
[ "${LOOP:-0}" = "1" ]   && FLAGS+=(--loop)
[ -n "${OFFSETS:-}" ]    && FLAGS+=(--offsets "$OFFSETS")
[ "${WINDOW:-0}" = "1" ] && FLAGS+=(--window)

info "Running: $(basename "$BIN") --replay (recorded videos)"
cd "$REPO_ROOT"   # so dartserver.db lands at the repo root, like dev runs
exec "$BIN" --replay \
  "$VIDEO0" "$VIDEO1" "$VIDEO2" \
  "$CALIB0" "$CALIB1" "$CALIB2" \
  ${FLAGS[@]+"${FLAGS[@]}"} "$@"
