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

# Retrieve raylib headers (for type definitions); the mock provides all implementations.
get_target_property(_RAYLIB_INCLUDE_DIR raylib INTERFACE_INCLUDE_DIRECTORIES)

add_executable(copilot-buddy-tests
    test_config.cpp
    test_event_parser.cpp
    test_find_session.cpp
    test_anim_state.cpp
    test_render_logic.cpp
    test_renderers.cpp
    raylib_mock.cpp
    ../src/event_parser.cpp
    ../src/anim_state.cpp
    ../src/sprite_renderer.cpp
    ../src/text_renderer.cpp
    ../src/info_renderer.cpp
)

target_include_directories(copilot-buddy-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../externals
    ${_RAYLIB_INCLUDE_DIR}
)

# Link against GTest only — raylib_mock.cpp provides all GPU function stubs.
target_link_libraries(copilot-buddy-tests PRIVATE
    GTest::gtest_main
    nlohmann_json::nlohmann_json
)

if(APPLE)
    target_compile_definitions(copilot-buddy-tests PRIVATE GL_SILENCE_DEPRECATION)
elseif(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(copilot-buddy-tests PRIVATE Threads::Threads)
endif()

include(GoogleTest)
gtest_discover_tests(copilot-buddy-tests)
