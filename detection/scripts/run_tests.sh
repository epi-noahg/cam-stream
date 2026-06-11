#!/usr/bin/env bash
#
# Replay annotated test specs (made with ./annotate_videos.sh) through the
# pipeline and check the detected darts against the ground truth.
#
# Usage:
#   ./run_tests.sh                       # all specs in detection/tests/
#   ./run_tests.sh tests/foo.yml ...     # specific specs
#   ./run_tests.sh --tol 90 ...          # widen the match window (frames)
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

BIN="$(binary camdetect_runtest)"
TESTS_DIR="${TESTS_DIR:-$DETECTION_DIR/tests}"

extra=()
specs=()
while [ $# -gt 0 ]; do
    case "$1" in
        --tol) extra+=("--tol" "$2"); shift 2 ;;
        *)     specs+=("$1"); shift ;;
    esac
done
if [ ${#specs[@]} -eq 0 ]; then
    while IFS= read -r f; do specs+=("$f"); done \
        < <(ls "$TESTS_DIR"/*.yml 2>/dev/null)
fi
[ ${#specs[@]} -gt 0 ] || die "no test specs found in $TESTS_DIR — create one with ./annotate_videos.sh <name>"

fail=0
for spec in "${specs[@]}"; do
    echo "── $spec"
    "$BIN" "$spec" ${extra[@]+"${extra[@]}"} || fail=1
done

if [ "$fail" -eq 0 ]; then echo "all tests PASSED"; else echo "some tests FAILED"; fi
exit "$fail"
