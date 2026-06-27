#!/usr/bin/env bash
# Run the GoDartss web app (Next.js).
#
#   scripts/run-web.sh [dev|release]   (default: dev)
#
#   dev      → next dev   (hot reload)
#   release  → next start (serves the production build from build-web.sh release)
#
# Env:
#   PORT   listening port (default 3000)
#   HOST   bind address  (default 0.0.0.0 so phones/tablets on the LAN can reach it)

source "$(dirname "$0")/lib/common.sh"

PROFILE="$(parse_profile "${1:-dev}")"
PM="$(detect_pm)"
PORT="${PORT:-3000}"
HOST="${HOST:-0.0.0.0}"

require_cmd node "Install Node.js 20+"
cd "$WEB_DIR"

if [ "$PROFILE" = "release" ]; then
  [ -d "$WEB_DIR/.next" ] || die "no production build found — run: scripts/build-web.sh release"
  info "Starting Next.js (production) on http://$HOST:$PORT"
  exec "$PM" run start -- --hostname "$HOST" --port "$PORT"
else
  info "Starting Next.js (dev) on http://$HOST:$PORT"
  exec "$PM" run dev -- --hostname "$HOST" --port "$PORT"
fi
