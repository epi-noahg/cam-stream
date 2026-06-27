#!/usr/bin/env bash
# Shared helpers for the build/run scripts. Source this from every script:
#   source "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
#
# Conventions:
#   • First positional argument selects the profile: "dev" or "release".
#   • Paths to calibration / videos / server URL are overridable via env vars
#     (see scripts/README.md). Sensible repo defaults are used otherwise.

set -euo pipefail

# ── Locations ────────────────────────────────────────────────────────────────
# This file lives in <repo>/scripts/lib, so the repo root is two levels up.
LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$LIB_DIR/../.." && pwd)"

WEB_DIR="$REPO_ROOT/GoDartss"
ANDROID_DIR="$WEB_DIR/android"
SERVER_SRC="$REPO_ROOT/dartserver"
DETECTION_SRC="$REPO_ROOT/detection"

# ── Logging ──────────────────────────────────────────────────────────────────
if [ -t 1 ]; then
  _C_BLUE='\033[1;34m'; _C_GREEN='\033[1;32m'; _C_YEL='\033[1;33m'
  _C_RED='\033[1;31m'; _C_DIM='\033[2m'; _C_OFF='\033[0m'
else
  _C_BLUE=''; _C_GREEN=''; _C_YEL=''; _C_RED=''; _C_DIM=''; _C_OFF=''
fi
info()  { printf "${_C_BLUE}▶ %s${_C_OFF}\n" "$*"; }
ok()    { printf "${_C_GREEN}✔ %s${_C_OFF}\n" "$*"; }
warn()  { printf "${_C_YEL}⚠ %s${_C_OFF}\n" "$*" >&2; }
die()   { printf "${_C_RED}✗ %s${_C_OFF}\n" "$*" >&2; exit 1; }
run()   { printf "${_C_DIM}\$ %s${_C_OFF}\n" "$*"; "$@"; }

# ── Helpers ──────────────────────────────────────────────────────────────────
# parse_profile <arg> → echoes "dev" or "release" (default: release).
parse_profile() {
  local p="${1:-release}"
  case "$p" in
    dev|debug)      echo "dev" ;;
    release|prod)   echo "release" ;;
    *) die "unknown profile '$p' (use: dev | release)" ;;
  esac
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "'$1' not found in PATH. ${2:-}"
}

nproc_safe() { command -v nproc >/dev/null 2>&1 && nproc || echo 4; }

# Node package manager: prefer pnpm, fall back to npm.
detect_pm() {
  if command -v pnpm >/dev/null 2>&1; then echo pnpm
  elif command -v npm >/dev/null 2>&1; then echo npm
  else die "neither pnpm nor npm found — install Node.js (https://nodejs.org)"; fi
}
# "exec a binary from node_modules" (cap/prisma) for the detected PM.
pm_exec() { case "$(detect_pm)" in pnpm) echo "pnpm exec";; *) echo "npx";; esac; }

# ── Server build dir for a profile ───────────────────────────────────────────
server_build_dir() { [ "$1" = "dev" ] && echo "$SERVER_SRC/build-debug" || echo "$SERVER_SRC/build"; }

# Resolve the dartserver binary to run. Honours BUILD=dev|release, otherwise
# prefers the release build, falling back to the debug build.
resolve_server_bin() {
  local prof="${BUILD:-}"
  if [ -n "$prof" ]; then
    local d; d="$(server_build_dir "$(parse_profile "$prof")")"
    [ -x "$d/dartserver" ] || die "no binary at $d/dartserver — run scripts/build-server.sh ${prof} first"
    echo "$d/dartserver"; return
  fi
  if   [ -x "$SERVER_SRC/build/dartserver" ];       then echo "$SERVER_SRC/build/dartserver"
  elif [ -x "$SERVER_SRC/build-debug/dartserver" ]; then echo "$SERVER_SRC/build-debug/dartserver"
  else die "dartserver not built — run: scripts/build-server.sh release"; fi
}

# ── Default calibration / video paths (overridable via env) ──────────────────
default_calib() {
  CALIB0="${CALIB0:-$REPO_ROOT/detection/cam0.yml}"
  CALIB1="${CALIB1:-$REPO_ROOT/detection/cam1.yml}"
  CALIB2="${CALIB2:-$REPO_ROOT/detection/cam2.yml}"
  for f in "$CALIB0" "$CALIB1" "$CALIB2"; do
    [ -f "$f" ] || die "calibration file not found: $f (set CALIB0/1/2)"
  done
}
default_videos() {
  VIDEO0="${VIDEO0:-$(ls "$REPO_ROOT"/client/cam0_*.mp4 2>/dev/null | head -1)}"
  VIDEO1="${VIDEO1:-$(ls "$REPO_ROOT"/client/cam1_*.mp4 2>/dev/null | head -1)}"
  VIDEO2="${VIDEO2:-$(ls "$REPO_ROOT"/client/cam2_*.mp4 2>/dev/null | head -1)}"
  for f in "$VIDEO0" "$VIDEO1" "$VIDEO2"; do
    [ -n "$f" ] && [ -f "$f" ] || die "replay video not found (set VIDEO0/1/2 to your recordings)"
  done
}
