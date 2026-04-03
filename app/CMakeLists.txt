cmake_minimum_required(VERSION 3.15)
project(copilot-buddy VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------- Dependencies via FetchContent ----------
include(FetchContent)

# raylib 5.5
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# nlohmann/json (header-only)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

# efsw (cross-platform file watcher)
FetchContent_Declare(efsw
    GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
    GIT_TAG 1.4.1
    GIT_SHALLOW TRUE
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(efsw)

# ---------- Main executable ----------
add_executable(copilot-buddy
    src/main.cpp
    src/status_monitor.cpp
    src/sprite_renderer.cpp
)

target_include_directories(copilot-buddy PRIVATE include)

target_link_libraries(copilot-buddy PRIVATE
    raylib
    nlohmann_json::nlohmann_json
    efsw
)

# Copy sprite assets next to binary
add_custom_command(TARGET copilot-buddy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:copilot-buddy>/resources
    COMMENT "Copying resources to build directory"
)

# macOS: suppress deprecation warnings for OpenGL
if(APPLE)
    target_compile_definitions(copilot-buddy PRIVATE GL_SILENCE_DEPRECATION)
endif()
