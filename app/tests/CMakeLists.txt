cmake_minimum_required(VERSION 3.15)

# Try a pre-installed GTest first (e.g. from Homebrew / system package).
# Fall back to downloading via FetchContent (used in CI).
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/releases/download/v1.14.0/googletest-1.14.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    # Prevent overriding the parent project's compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(copilot-buddy-tests
    test_config.cpp
    test_event_parser.cpp
    test_find_session.cpp
    ../src/event_parser.cpp
)

target_include_directories(copilot-buddy-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../externals
)

target_link_libraries(copilot-buddy-tests PRIVATE
    GTest::gtest_main
    nlohmann_json::nlohmann_json
    raylib
)

if(APPLE)
    target_compile_definitions(copilot-buddy-tests PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(copilot-buddy-tests PRIVATE
        "-framework CoreFoundation" "-framework CoreServices"
        "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
elseif(WIN32)
    target_link_libraries(copilot-buddy-tests PRIVATE opengl32 gdi32 winmm)
    if(MSVC)
        target_compile_definitions(copilot-buddy-tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
else()
    find_package(Threads REQUIRED)
    target_link_libraries(copilot-buddy-tests PRIVATE Threads::Threads GL m dl rt X11)
endif()

include(GoogleTest)
gtest_discover_tests(copilot-buddy-tests)
