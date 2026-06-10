#!/usr/bin/env bash
#
# Connect to the camstream server and run the live debug UI (camdetect_live).
# Loads camN.yml from CALIB_DIR; the pixel-accurate camN_zones.png is picked up
# automatically when it sits next to the yml.
#
# Usage:
#   ./debug_live.sh <server-ip> [port]
#   CAMSTREAM_HOST=192.168.1.42 ./debug_live.sh        # host/port via env
#
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

BIN="$(binary camdetect_live)"

HOST="${1:-${CAMSTREAM_HOST:-}}"
PORT="${2:-${CAMSTREAM_PORT:-}}"
[ -n "$HOST" ] || die "no server IP — pass it as arg 1 or set CAMSTREAM_HOST"

args=("$HOST")
[ -n "$PORT" ] && args+=("$PORT")
for c in $(cam_indices); do
    y="$(cam_yml "$c")"
    [ -f "$y" ] || die "missing $y — run ./setup_cams.sh first"
    args+=("$y")
done

echo ">> connecting to $HOST${PORT:+:$PORT}"
exec "$BIN" "${args[@]}"
