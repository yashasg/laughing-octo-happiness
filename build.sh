#!/usr/bin/env bash
# CI build script — Linux / macOS
# Usage: ./build.sh [-test]
#   -test  also build and run unit tests, writing results to test-results.xml
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$REPO_ROOT/app/build"
BUILD_TESTS=OFF

for arg in "$@"; do
    case "$arg" in
        -test) BUILD_TESTS=ON ;;
    esac
done

cmake -S "$REPO_ROOT/app" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DBUILD_TESTS="$BUILD_TESTS"
cmake --build "$BUILD_DIR" --parallel

echo "Build succeeded: $BUILD_DIR/copilot-buddy"

if [ "$BUILD_TESTS" = "ON" ]; then
    cmake --build "$BUILD_DIR" --target copilot-buddy-tests --parallel
    # On Windows (MSYS2) the binary has a .exe suffix
    TEST_BIN="$BUILD_DIR/tests/copilot-buddy-tests"
    [ -f "${TEST_BIN}.exe" ] && TEST_BIN="${TEST_BIN}.exe"
    # Native Windows binaries don't translate POSIX paths; use cygpath if available
    if command -v cygpath >/dev/null 2>&1; then
        RESULTS_PATH="$(cygpath -w "$BUILD_DIR")/test-results.xml"
    else
        RESULTS_PATH="$BUILD_DIR/test-results.xml"
    fi
    "$TEST_BIN" --gtest_output="xml:$RESULTS_PATH"
    echo "Tests passed."
fi
