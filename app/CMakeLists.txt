cmake_minimum_required(VERSION 3.15)
project(copilot-buddy VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------- raylib 5.5: pre-built official release binaries ----------
# Download the pre-built static library for the current platform instead of
# cloning the full source repo and compiling from scratch.
include(FetchContent)

if(APPLE)
    set(RAYLIB_URL "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_macos.tar.gz")
    set(RAYLIB_LIB_NAME "libraylib.a")
elseif(WIN32)
    if(MSVC)
        set(RAYLIB_URL "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_msvc16.zip")
        set(RAYLIB_LIB_NAME "raylib.lib")
    else()
        set(RAYLIB_URL "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip")
        set(RAYLIB_LIB_NAME "libraylib.a")
    endif()
else()
    set(RAYLIB_URL "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_linux_amd64.tar.gz")
    set(RAYLIB_LIB_NAME "libraylib.a")
endif()

FetchContent_Declare(raylib URL ${RAYLIB_URL} DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(raylib)

# Locate the static lib and include dir inside the extracted archive.
# Archives have a top-level platform folder (e.g. raylib-5.5_macos/), so we
# search one level deep to stay robust across platforms.
file(GLOB_RECURSE _RAYLIB_LIB_CANDIDATES
    LIST_DIRECTORIES false
    "${raylib_SOURCE_DIR}/${RAYLIB_LIB_NAME}"
    "${raylib_SOURCE_DIR}/*/${RAYLIB_LIB_NAME}")
if(NOT _RAYLIB_LIB_CANDIDATES)
    message(FATAL_ERROR "raylib static library (${RAYLIB_LIB_NAME}) not found under ${raylib_SOURCE_DIR}")
endif()
list(GET _RAYLIB_LIB_CANDIDATES 0 _RAYLIB_LIB_PATH)
get_filename_component(_RAYLIB_LIB_DIR "${_RAYLIB_LIB_PATH}" DIRECTORY)
get_filename_component(_RAYLIB_PREBUILT_DIR "${_RAYLIB_LIB_DIR}" DIRECTORY)
set(_RAYLIB_INCLUDE_DIR "${_RAYLIB_PREBUILT_DIR}/include")

add_library(raylib STATIC IMPORTED GLOBAL)
set_target_properties(raylib PROPERTIES
    IMPORTED_LOCATION "${_RAYLIB_LIB_PATH}"
    INTERFACE_INCLUDE_DIRECTORIES "${_RAYLIB_INCLUDE_DIR}")

# Platform system libraries required by raylib
if(APPLE)
    set_property(TARGET raylib APPEND PROPERTY INTERFACE_LINK_LIBRARIES
        "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
elseif(WIN32)
    set_property(TARGET raylib APPEND PROPERTY INTERFACE_LINK_LIBRARIES
        opengl32 gdi32 winmm)
else()
    set_property(TARGET raylib APPEND PROPERTY INTERFACE_LINK_LIBRARIES
        GL m pthread dl rt X11)
endif()

# ---------- nlohmann/json 3.11.3: vendored single header ----------
# externals/nlohmann/json.hpp — no download, no compilation.
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/externals>)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)

# ---------- dmon: vendored single header ----------
# externals/dmon/dmon.h — no download, no separate compilation.
# DMON_IMPL is defined in status_monitor.cpp which owns the implementation.

# ---------- Main executable ----------
add_executable(copilot-buddy
    src/main.cpp
    src/status_monitor.cpp
    src/sprite_renderer.cpp
    src/anim_state.cpp
    src/text_renderer.cpp
    src/info_renderer.cpp
    src/input_handler.cpp
    src/event_parser.cpp
)

target_include_directories(copilot-buddy PRIVATE
    include
    externals
    externals/glfw/include)

target_link_libraries(copilot-buddy PRIVATE
    raylib
    nlohmann_json::nlohmann_json
)

target_compile_definitions(copilot-buddy PRIVATE
    COPILOT_BUDDY_VERSION="${PROJECT_VERSION}"
)

# Copy sprite assets next to binary
add_custom_command(TARGET copilot-buddy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:copilot-buddy>/resources
    COMMENT "Copying resources to build directory"
)

# macOS: suppress deprecation warnings for OpenGL; link dmon's CoreServices.
# _LIBCPP_REMOVE_TRANSITIVE_INCLUDES makes libc++ behave like libstdc++/MSVC —
# each TU must include exactly the headers it uses, keeping builds consistent.
if(APPLE)
    target_compile_definitions(copilot-buddy PRIVATE
        GL_SILENCE_DEPRECATION
        _LIBCPP_REMOVE_TRANSITIVE_INCLUDES)
    target_link_libraries(copilot-buddy PRIVATE
        "-framework CoreFoundation" "-framework CoreServices")
elseif(UNIX)
    # dmon uses pthreads on Linux
    find_package(Threads REQUIRED)
    target_link_libraries(copilot-buddy PRIVATE Threads::Threads)
endif()

# ---------- Size optimisation (Release / MinSizeRel only) ----------
# LTO lets the linker see across TUs and dead-strip unused raylib/nlohmann code.
# Measured reduction: ~540 KB from ~1.1 MB on macOS arm64 (−51%).
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    target_compile_options(copilot-buddy PRIVATE
        -ffunction-sections -fdata-sections -flto=thin)
    if(APPLE)
        target_link_options(copilot-buddy PRIVATE -flto=thin -Wl,-dead_strip)
    elseif(WIN32)
        # MinGW clang: use lld for native thin-LTO support (GNU ld's gold
        # plugin fails to load in MSYS2 environments).
        target_link_options(copilot-buddy PRIVATE -flto=thin -fuse-ld=lld -Wl,--gc-sections)
    elseif(UNIX)
        target_link_options(copilot-buddy PRIVATE -flto=thin -Wl,--gc-sections)
    endif()
endif()

# ---------- Tests ----------
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
