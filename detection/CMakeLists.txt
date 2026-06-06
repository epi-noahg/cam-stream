cmake_minimum_required(VERSION 3.16)
project(camdetect CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenCV REQUIRED)

# ── Library ──────────────────────────────────────────────────────────────────
add_library(camdetect STATIC
    src/BoardCalibration.cpp
    src/BoardCalibrator.cpp
    src/DartDetector.cpp
    src/DebugUI.cpp
    src/MultiCamFusion.cpp
    src/Pipeline.cpp
    src/Renderer.cpp
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

add_executable(camdetect_offline tools/detect_offline.cpp)
target_link_libraries(camdetect_offline PRIVATE camdetect)

add_executable(camdetect_debug tools/debug_viewer.cpp)
target_link_libraries(camdetect_debug PRIVATE camdetect)
