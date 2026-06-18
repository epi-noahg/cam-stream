cmake_minimum_required(VERSION 3.16)
project(dartserver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

enable_testing()

# ─────────────────────────────────────────────────────────────────────────────
# dartgame — pure X01 rules + checkout solver.  No external dependencies, so it
# builds fast and is unit-tested in isolation.  Other layers (detection, api,
# persistence) link against it.
# ─────────────────────────────────────────────────────────────────────────────
add_library(dartgame STATIC
    src/game/X01Engine.cpp
    src/game/Checkout.cpp
    src/game/GameManager.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(dartgame PUBLIC Threads::Threads)
target_include_directories(dartgame PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(dartgame PRIVATE -Wall -Wextra)

# ── Tests ────────────────────────────────────────────────────────────────────
add_executable(test_x01 tests/test_x01.cpp)
target_link_libraries(test_x01 PRIVATE dartgame)
add_test(NAME x01_engine COMMAND test_x01)

add_executable(test_game_manager tests/test_game_manager.cpp)
target_link_libraries(test_game_manager PRIVATE dartgame)
add_test(NAME game_manager COMMAND test_game_manager)

# ─────────────────────────────────────────────────────────────────────────────
# dartdetect — bridges the camdetect Pipeline to the game.  Pulls in OpenCV and
# the camdetect static library (built from ../detection), plus CameraCapture
# (shared by include path from ../server/src).
# ─────────────────────────────────────────────────────────────────────────────
find_package(OpenCV REQUIRED)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../detection
                 ${CMAKE_BINARY_DIR}/detection EXCLUDE_FROM_ALL)

add_library(dartdetect STATIC
    src/detection/DetectionTypes.cpp
    src/detection/DetectionService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/src/CameraCapture.cpp
)
target_include_directories(dartdetect PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../detection      # for "sources/FileSource.hpp"
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/src     # for "CameraCapture.hpp"
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(dartdetect PUBLIC
    dartgame camdetect ${OpenCV_LIBS} Threads::Threads
)
target_compile_options(dartdetect PRIVATE -Wall -Wextra)

add_executable(test_zone tests/test_zone.cpp)
target_link_libraries(test_zone PRIVATE dartdetect)
add_test(NAME zone_parse COMMAND test_zone)

# ─────────────────────────────────────────────────────────────────────────────
# dartapi — WebSocket + JSON protocol layer (Boost.Beast + nlohmann/json).
# ─────────────────────────────────────────────────────────────────────────────
find_package(nlohmann_json REQUIRED)
find_package(Boost REQUIRED)

add_library(dartapi STATIC
    src/api/Json.cpp
    src/api/WsServer.cpp
    src/api/Dispatcher.cpp
)
target_include_directories(dartapi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(dartapi PUBLIC
    dartgame dartdetect nlohmann_json::nlohmann_json Threads::Threads
)
target_compile_options(dartapi PRIVATE -Wall -Wextra)

# ─────────────────────────────────────────────────────────────────────────────
# dartpersist — SQLite persistence (players, finished matches, stats).
# ─────────────────────────────────────────────────────────────────────────────
find_package(SQLite3 REQUIRED)

add_library(dartpersist STATIC src/persistence/Db.cpp)
target_include_directories(dartpersist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(dartpersist PUBLIC dartgame SQLite::SQLite3)
target_compile_options(dartpersist PRIVATE -Wall -Wextra)

add_executable(test_db tests/test_db.cpp)
target_link_libraries(test_db PRIVATE dartpersist)
add_test(NAME persistence COMMAND test_db)

# ── Server executable: capture/replay → detection → game → WebSocket API ─────
add_executable(dartserver src/main.cpp)
target_link_libraries(dartserver PRIVATE dartapi dartdetect dartgame dartpersist)
