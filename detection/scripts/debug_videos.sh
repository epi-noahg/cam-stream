#!/usr/bin/env bash
#
# Replay the recorded videos through the pipeline.  Loads camN.yml + the
# pixel-accurate camN_zones.png (auto-detected next to each yml).
#
# Default opens the live debug UI (camdetect_debug).  --no-ui runs the headless
# scorer (camdetect_offline), printing each fused hit to stdout.
#
# Usage:
#   ./debug_videos.sh            # debug UI
#   ./debug_videos.sh --no-ui    # headless, prints hits
#
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

UI=1
[ "${1:-}" = "--no-ui" ] && UI=0

if [ "$UI" -eq 1 ]; then BIN="$(binary camdetect_debug)"; else BIN="$(binary camdetect_offline)"; fi

# Both tools take: cam0.mp4 cam1.mp4 cam2.mp4  cam0.yml cam1.yml cam2.yml
videos=()
ymls=()
for c in $(cam_indices); do
    v="$(cam_video "$c")"
    [ -f "$v" ] || die "no recording for cam$c (looked in $VIDEO_DIR)"
    y="$(cam_yml "$c")"
    [ -f "$y" ] || die "missing $y — run ./setup_cams.sh first"
    videos+=("$v")
    ymls+=("$y")
done

exec "$BIN" "${videos[@]}" "${ymls[@]}"
