cmake_minimum_required(VERSION 3.16)
project(camdetect CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenCV REQUIRED)
find_package(PkgConfig)        # required only by camdetect_live
find_package(Threads)          # required only by camdetect_live

# FFmpeg is needed for the live-mode tool that decodes H.264 packets from the
# camstream server.  Mark it optional so the file-mode tools still build on
# systems without FFmpeg dev headers.
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG IMPORTED_TARGET
        libavcodec
        libavutil
        libswscale
    )
endif()

# ── Library ──────────────────────────────────────────────────────────────────
add_library(camdetect STATIC
    src/AutoCalibrator.cpp
    src/BoardCalibration.cpp
    src/BoardCalibrator.cpp
    src/DartDetector.cpp
    src/DebugUI.cpp
    src/MultiCamFusion.cpp
    src/Pipeline.cpp
    src/Renderer.cpp
    src/ZoneMap.cpp
    src/ZoneMapper.cpp
    sources/FileSource.cpp
)

target_include_directories(camdetect PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(camdetect PUBLIC ${OpenCV_LIBS})
target_compile_options(camdetect PRIVATE -Wall -Wextra)

# ── Tools ────────────────────────────────────────────────────────────────────
add_executable(camdetect_calibrate tools/calibrate.cpp)
target_link_libraries(camdetect_calibrate PRIVATE camdetect)

add_executable(camdetect_autocalib tools/autocalib.cpp)
target_link_libraries(camdetect_autocalib PRIVATE camdetect)

add_executable(camdetect_offline tools/detect_offline.cpp)
target_link_libraries(camdetect_offline PRIVATE camdetect)

add_executable(camdetect_debug tools/debug_viewer.cpp)
target_link_libraries(camdetect_debug PRIVATE camdetect)

# ── Live mode: connects to the camstream server TCP stream ───────────────────
# Reuses the client's StreamReceiver + VideoDecoder source files directly so we
# don't have to refactor the client into a shared library.
set(CLIENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../client)
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
if(FFMPEG_FOUND AND Threads_FOUND
   AND EXISTS ${CLIENT_DIR}/src/StreamReceiver.cpp
   AND EXISTS ${CLIENT_DIR}/src/VideoDecoder.cpp)
    add_executable(camdetect_live
        tools/live_viewer.cpp
        ${CLIENT_DIR}/src/StreamReceiver.cpp
        ${CLIENT_DIR}/src/VideoDecoder.cpp
    )
    target_include_directories(camdetect_live PRIVATE
        ${CLIENT_DIR}/src
        ${COMMON_DIR}
    )
    target_link_libraries(camdetect_live PRIVATE
        camdetect
        PkgConfig::FFMPEG
        Threads::Threads
    )
else()
    message(STATUS
        "camdetect_live skipped (need FFmpeg dev headers + ../client sources)")
endif()
