# scripts/

Build & run helpers for the three deliverables — the **C++ dart server**, the
**GoDartss web app**, and the **Android APK**. Each build script takes a profile
argument: `dev` or `release`. Tested for an Ubuntu server.

```
scripts/
├── build-server.sh      # C++ server   (dev=Debug+tests / release=Release)
├── build-web.sh         # Next.js app  (dev=deps+prisma / release=+next build)
├── build-apk.sh         # Android APK  (dev=assembleDebug / release=assembleRelease)
├── run-web.sh           # serve the web app (dev=next dev / release=next start)
├── run-server-cam.sh    # run the server on live cameras   (--live)
├── run-server-video.sh  # run the server on recorded videos (--replay)
└── lib/common.sh        # shared helpers (sourced by all scripts)
```

## Prerequisites (Ubuntu)

```bash
# C++ server
sudo apt install -y build-essential cmake pkg-config \
    libopencv-dev libboost-all-dev libsqlite3-dev nlohmann-json3-dev

# Web app + APK tooling
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash - && sudo apt install -y nodejs
sudo npm i -g pnpm           # optional; npm works too

# APK only: JDK + Android SDK
sudo apt install -y openjdk-17-jdk
# install the Android cmdline-tools and `export ANDROID_HOME=...`
# (or put `sdk.dir=/path/to/Android/Sdk` in GoDartss/android/local.properties)
```

## Build

```bash
scripts/build-server.sh release     # → dartserver/build/dartserver
scripts/build-web.sh    release     # → GoDartss/.next
GODARTS_SERVER_URL=http://<server-ip>:3000 scripts/build-apk.sh release
```

`dev` profiles: `build-server.sh dev` builds Debug and runs the unit tests;
`build-web.sh dev` just installs deps + generates the Prisma client.

## Run

```bash
# Web UI (bind 0.0.0.0 so tablets/phones on the LAN can reach it)
scripts/run-web.sh release            # or: dev

# Server — pick ONE of:
scripts/run-server-cam.sh             # live cameras  (--live)
scripts/run-server-video.sh           # recorded clips (--replay), no hardware
LOOP=1 scripts/run-server-video.sh    # loop the demo videos
```

The server exposes its WebSocket API on **:8080**; the web app talks to it
automatically (same host, port 8080). The web app serves on **:3000**.

## Useful environment variables

| Var | Used by | Default |
| --- | --- | --- |
| `PORT`, `HOST` | run-web | `3000`, `0.0.0.0` |
| `BUILD` | run-server-* | auto (release if built, else dev) |
| `CALIB0/1/2` | run-server-* | `detection/cam{0,1,2}.yml` |
| `DEVICES` | run-server-cam | OpenCV auto-index (3 cams) |
| `VIDEO0/1/2` | run-server-video | `client/cam{0,1,2}_*.mp4` |
| `LOOP`, `OFFSETS`, `WINDOW` | run-server-video | off |
| `GODARTS_SERVER_URL` | build-apk | `http://192.168.1.100:3000` |

## Typical Ubuntu session

```bash
scripts/build-server.sh release
scripts/build-web.sh    release
scripts/run-server-video.sh &     # or run-server-cam.sh with real cameras
scripts/run-web.sh release        # open http://<server-ip>:3000 on a tablet
```
