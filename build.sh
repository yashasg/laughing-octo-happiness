#!/usr/bin/env bash
# CI build script — Linux / macOS
# Builds copilot-buddy in Release mode. Does not run the binary.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$REPO_ROOT/app/build"

cmake -S "$REPO_ROOT/app" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel

echo "Build succeeded: $BUILD_DIR/copilot-buddy"
