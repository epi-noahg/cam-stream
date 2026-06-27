#!/usr/bin/env bash
# Build the GoDarts Android APK (Capacitor wrapper around the Next.js app).
#
#   scripts/build-apk.sh [dev|release]   (default: dev)
#
#   dev      → assembleDebug   → app-debug.apk
#   release  → assembleRelease → app-release-unsigned.apk (sign before publishing)
#
# The APK is a thin shell that loads the web UI from GODARTS_SERVER_URL at
# runtime (see GoDartss/capacitor.config.ts). Point it at the machine running
# `scripts/run-web.sh` (the box with the cameras):
#
#   GODARTS_SERVER_URL=http://192.168.1.42:3000 scripts/build-apk.sh release
#
# Ubuntu deps: JDK 17+ and the Android SDK (cmdline-tools + platform/build-tools).
# Set ANDROID_HOME (or create GoDartss/android/local.properties with sdk.dir=...).

source "$(dirname "$0")/lib/common.sh"

PROFILE="$(parse_profile "${1:-dev}")"
[ "$PROFILE" = "dev" ] && GRADLE_TASK="assembleDebug" || GRADLE_TASK="assembleRelease"

PM="$(detect_pm)"; PX="$(pm_exec)"
require_cmd node "Install Node.js 20+"
command -v java >/dev/null 2>&1 || warn "java not found — install a JDK 17+ (sudo apt install -y openjdk-17-jdk)"
if [ -z "${ANDROID_HOME:-}" ] && [ ! -f "$ANDROID_DIR/local.properties" ]; then
  warn "ANDROID_HOME is unset and $ANDROID_DIR/local.properties is missing — Gradle may fail to find the SDK"
fi

export GODARTS_SERVER_URL="${GODARTS_SERVER_URL:-http://192.168.1.100:3000}"
info "APK will load the UI from: $GODARTS_SERVER_URL"

cd "$WEB_DIR"
# Capacitor needs a webDir even in remote-URL mode; keep a placeholder present.
[ -f "$WEB_DIR/www/index.html" ] || { mkdir -p "$WEB_DIR/www"; echo "<!doctype html><title>GoDarts</title>" > "$WEB_DIR/www/index.html"; }

info "Installing dependencies with $PM"
run "$PM" install

info "Syncing Capacitor Android project"
run $PX cap sync android

info "Gradle $GRADLE_TASK"
chmod +x "$ANDROID_DIR/gradlew"
( cd "$ANDROID_DIR" && run ./gradlew "$GRADLE_TASK" )

APK="$(find "$ANDROID_DIR/app/build/outputs/apk" -name '*.apk' -newer "$ANDROID_DIR/gradlew" 2>/dev/null | head -1)"
[ -n "$APK" ] && ok "APK built: $APK" || ok "Build finished — APK under $ANDROID_DIR/app/build/outputs/apk/"
