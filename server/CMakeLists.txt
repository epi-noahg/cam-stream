cmake_minimum_required(VERSION 3.16)
project(camstream_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependencies ──────────────────────────────────────────────────────────────

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)

# FFmpeg libraries required for H.264 encoding
pkg_check_modules(FFMPEG REQUIRED
    libavcodec
    libavutil
    libswscale
)

# ── Target ────────────────────────────────────────────────────────────────────

add_executable(camstream_server
    src/main.cpp
    src/CameraCapture.cpp
    src/VideoEncoder.cpp
    src/StreamServer.cpp
)

target_include_directories(camstream_server PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/../common   # Protocol.hpp
    ${OpenCV_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(camstream_server PRIVATE
    ${OpenCV_LIBS}
    ${FFMPEG_LIBRARIES}
    Threads::Threads
)

# Pass pkg-config compile flags (e.g. -D_REENTRANT on some distros)
target_compile_options(camstream_server PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    -Wall -Wextra -Wpedantic
)

find_package(Threads REQUIRED)
