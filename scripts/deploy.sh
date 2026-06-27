#!/usr/bin/env bash
# Auto-deploy: fetch the tracked branch, and if it moved, rebuild only the
# components that changed (C++ server and/or Next.js web) then restart their
# systemd services. Designed to be run on a 60s systemd timer (polling), but is
# also safe to run by hand.
#
#   scripts/deploy.sh            # deploy if there are new commits, else no-op
#   scripts/deploy.sh --force    # rebuild + restart everything unconditionally
#
# It restarts these units (must exist on the host):
#   dartserver.service        — C++ detection/game server (WS :8080)
#   dartserver-web.service    — Next.js web app (:3000)
#
# Restarting requires sudo; grant it without a password via the sudoers drop-in
# in scripts/deploy/dartserver-deploy.sudoers. Run as the repo owner (pstq) so
# the build artifacts stay owned by that user.

source "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"

FORCE=0
[ "${1:-}" = "--force" ] && FORCE=1

SERVER_UNIT="dartserver.service"
WEB_UNIT="dartserver-web.service"

cd "$REPO_ROOT"

# ── Single-instance lock: never let two deploys overlap (a `next build` can run
# longer than the timer interval). flock releases automatically on exit. ───────
exec 9>"$REPO_ROOT/.deploy.lock"
if ! flock -n 9; then
  info "another deploy is running — skipping"
  exit 0
fi

require_cmd git

BRANCH="$(git symbolic-ref --short -q HEAD || true)"
[ -n "$BRANCH" ] || die "repo is in detached HEAD — checkout the deploy branch first"
git rev-parse --abbrev-ref "@{u}" >/dev/null 2>&1 \
  || die "branch '$BRANCH' has no upstream — set one with: git branch --set-upstream-to=origin/$BRANCH"

info "fetching origin ($BRANCH)…"
run git fetch --quiet origin "$BRANCH"

OLD="$(git rev-parse HEAD)"
NEW="$(git rev-parse "@{u}")"

if [ "$FORCE" -eq 0 ] && [ "$OLD" = "$NEW" ]; then
  # Up to date — stay quiet so the journal isn't spammed every minute.
  exit 0
fi

# What changed between the deployed commit and the incoming one?
if [ "$FORCE" -eq 1 ]; then
  CHANGED="dartserver/ GoDartss/"
else
  CHANGED="$(git diff --name-only "$OLD" "$NEW")"
fi

info "deploying $OLD → $NEW"
# Mirror the remote exactly (deploy box should not carry local commits/edits).
run git reset --hard "$NEW"

needs() { grep -q "$1" <<<"$CHANGED"; }

DID_SERVER=0
DID_WEB=0

if needs "^dartserver/" || [ "$FORCE" -eq 1 ]; then
  info "C++ server changed → rebuilding"
  run "$REPO_ROOT/scripts/build-server.sh" release
  DID_SERVER=1
fi

if needs "^GoDartss/" || [ "$FORCE" -eq 1 ]; then
  info "web app changed → rebuilding"
  run "$REPO_ROOT/scripts/build-web.sh" release
  DID_WEB=1
fi

if [ "$DID_SERVER" -eq 1 ]; then
  info "restarting $SERVER_UNIT"
  run sudo systemctl restart "$SERVER_UNIT"
fi
if [ "$DID_WEB" -eq 1 ]; then
  info "restarting $WEB_UNIT"
  run sudo systemctl restart "$WEB_UNIT"
fi

if [ "$DID_SERVER" -eq 0 ] && [ "$DID_WEB" -eq 0 ]; then
  ok "pulled new commits, but no buildable component changed — nothing to restart"
else
  ok "deploy complete (server=$DID_SERVER web=$DID_WEB)"
fi
