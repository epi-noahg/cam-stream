cmake_minimum_required(VERSION 3.16)
project(camstream_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependencies ──────────────────────────────────────────────────────────────

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# FFmpeg libraries required for H.264 decoding
pkg_check_modules(FFMPEG REQUIRED
    libavcodec
    libavutil
    libswscale
)

# ── Target ────────────────────────────────────────────────────────────────────

add_executable(camstream_client
    src/main.cpp
    src/StreamReceiver.cpp
    src/VideoDecoder.cpp
    src/Display.cpp
)

target_include_directories(camstream_client PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/../common   # Protocol.hpp
    ${OpenCV_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(camstream_client PRIVATE
    ${OpenCV_LIBS}
    ${FFMPEG_LIBRARIES}
    Threads::Threads
)

target_compile_options(camstream_client PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    -Wall -Wextra -Wpedantic
)
