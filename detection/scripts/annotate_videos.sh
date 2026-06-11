#!/usr/bin/env bash
#
# Open the annotation UI (camdetect_annotate) on the recorded videos to build
# a ground-truth test spec: link the cams with the delay sliders, then mark
# expected darts ('d'), human-in-frame intervals ('h') and board-clear points
# ('c').  The spec is saved to detection/tests/<name>.yml and replayed as a
# unit test by ./run_tests.sh.
#
# Usage:
#   ./annotate_videos.sh <name>     # edits/creates tests/<name>.yml
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

[ $# -ge 1 ] || die "usage: $0 <test-name>   (writes tests/<test-name>.yml)"
TESTS_DIR="${TESTS_DIR:-$DETECTION_DIR/tests}"
mkdir -p "$TESTS_DIR"
SPEC="$TESTS_DIR/$1.yml"

BIN="$(binary camdetect_annotate)"

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

exec "$BIN" "${videos[@]}" "${ymls[@]}" "$SPEC"
