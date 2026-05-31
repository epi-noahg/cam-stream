# camstream

Stream three webcams from an Ubuntu server to a remote viewer over TCP.

```
┌─────────────────────────────┐          TCP          ┌──────────────────────┐
│  Ubuntu server              │ ─────────────────────▶ │  Mac / viewer        │
│  /dev/video0 ─▶ encoder ─┐  │                        │  decoder ─▶ display  │
│  /dev/video1 ─▶ encoder ─┤──│──  H.264 + protocol ──▶│  decoder ─▶ display  │
│  /dev/video2 ─▶ encoder ─┘  │                        │  decoder ─▶ display  │
└─────────────────────────────┘                        └──────────────────────┘
```

**Codec**: H.264 (libx264 encode / libavcodec decode), ~500 kbit/s per camera  
**Transport**: plain TCP, single connection, custom binary framing  
**Protocol**: `Init` packet (SPS/PPS + resolution) once per camera, then `Video` packets

---

## Directory layout

```
cam-stream/
├── common/
│   └── Protocol.hpp          # shared wire format (header + packet types)
├── server/
│   ├── CMakeLists.txt
│   └── src/
│       ├── CameraCapture.{hpp,cpp}  # V4L2 capture via OpenCV
│       ├── VideoEncoder.{hpp,cpp}   # H.264 encode via libx264/libavcodec
│       ├── StreamServer.{hpp,cpp}   # TCP server, multi-client broadcast
│       └── main.cpp
└── client/
    ├── CMakeLists.txt
    └── src/
        ├── StreamReceiver.{hpp,cpp} # TCP client, packet framing
        ├── VideoDecoder.{hpp,cpp}   # H.264 decode via libavcodec
        ├── Display.{hpp,cpp}        # composite 3-up viewer (OpenCV)
        └── main.cpp
```

---

## Server – Ubuntu

### Install dependencies

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake \
    libopencv-dev \
    libavcodec-dev libavformat-dev libswscale-dev libavutil-dev \
    libx264-dev
```

### Build

```bash
cd server
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Run

```bash
./build/camstream_server
# optional: pass device paths explicitly
./build/camstream_server /dev/video0 /dev/video1 /dev/video2
```

The server listens on port **8554** by default.  
To stream over the internet, forward port 8554 (TCP) on your router to the server PC.

---

## Client – macOS

### Install dependencies

```bash
brew install cmake opencv ffmpeg
```

### Build

```bash
cd client
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Run

```bash
./build/camstream_client <server-ip>
# custom port
./build/camstream_client 203.0.113.42 8554
```

A single window opens showing all three camera feeds side-by-side.  
Press **ESC** or close the window to quit.

---

## Tuning

| Parameter | Location | Default |
|-----------|----------|---------|
| Resolution | `server/src/main.cpp` `WIDTH` / `HEIGHT` | 640 × 480 |
| Frame rate | `server/src/main.cpp` `FPS` | 30 |
| Bitrate per camera | `server/src/main.cpp` `BITRATE_KBPS` | 500 kbit/s |
| Server port | `common/Protocol.hpp` `DEFAULT_PORT` | 8554 |

Total bandwidth (3 cameras × 500 kbit/s) is roughly **1.5 Mbit/s** upstream from the server.

---

## Android (future)

The client uses only POSIX sockets, libavcodec, and OpenCV – all available on Android via NDK.  
Replace `Display` with an Android `SurfaceView`/`ANativeWindow` renderer and adapt `main.cpp` to your activity lifecycle.
