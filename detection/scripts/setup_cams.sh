#!/usr/bin/env bash
#
# Auto-calibrate the cameras → writes camN.yml + camN_zones.png into CALIB_DIR.
#
# Default is INTERACTIVE: each camera's recording opens so you can check the
# coloured zone overlay and, if the sector numbering is rotated, CLICK inside
# sector 20.  Press 's' to save, 'q' for the next camera.
#
# --batch runs headless using the stored sector-20 hints below (tuned for the
# bundled test recordings) — fast, but verify the *_overlay.png afterwards.
#
# Usage:
#   ./setup_cams.sh                 # interactive, all cameras
#   ./setup_cams.sh --batch         # headless, all cameras, stored hints
#   ./setup_cams.sh --frame 12 1    # only cam1, seek to frame 12
#
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

BIN="$(binary camdetect_autocalib)"

# Stored sector-20 hints for the bundled recordings (pixel inside sector 20).
# Empty = let auto-orientation decide.  Override per rig as needed.
hint_for() {
    case "$1" in
        1) echo "--hint 324 268" ;;
        2) echo "--hint 110 195" ;;
        *) echo "" ;;
    esac
}

BATCH=0
FRAME=5
CAMS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --batch)      BATCH=1 ;;
        --frame)      FRAME="$2"; shift ;;
        [0-9]*)       CAMS+=("$1") ;;
        -h|--help)    sed -n '2,20p' "$0"; exit 0 ;;
        *)            die "unknown argument: $1" ;;
    esac
    shift
done
[ ${#CAMS[@]} -gt 0 ] || CAMS=($(cam_indices))

for c in "${CAMS[@]}"; do
    video="$(cam_video "$c")"
    [ -f "$video" ] || die "no recording for cam$c (looked in $VIDEO_DIR)"
    out="$(cam_yml "$c")"
    echo ">> cam$c: $video → $out"
    if [ "$BATCH" -eq 1 ]; then
        # shellcheck disable=SC2046  # intentional word-split of the hint flag
        "$BIN" "$video" "$out" --batch --frame "$FRAME" $(hint_for "$c")
    else
        "$BIN" "$video" "$out" --frame "$FRAME"
    fi
done

echo "done — calibrations in $CALIB_DIR"
