#!/usr/bin/env bash
# Build the GoDartss web app (Next.js).
#
#   scripts/build-web.sh [dev|release]   (default: dev)
#
#   dev      → install deps + generate the Prisma client (ready for `next dev`)
#   release  → install deps + Prisma client + production build (`next build`)
#
# Ubuntu deps: Node.js 20+ and pnpm (or npm).
#   curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash - && sudo apt install -y nodejs

source "$(dirname "$0")/lib/common.sh"

PROFILE="$(parse_profile "${1:-dev}")"
PM="$(detect_pm)"; PX="$(pm_exec)"

require_cmd node "Install Node.js 20+ (https://nodejs.org)"
cd "$WEB_DIR"

info "Installing dependencies with $PM"
run "$PM" install

info "Generating Prisma client"
run $PX prisma generate

if [ "$PROFILE" = "release" ]; then
  info "Production build (next build)"
  run "$PM" run build
  ok "Web app built (release). Serve it with: scripts/run-web.sh release"
else
  ok "Web app ready (dev). Start it with: scripts/run-web.sh dev"
fi
