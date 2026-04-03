#!/usr/bin/env bash
# Copilot Buddy — build and launch (macOS / Linux)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bash "$SCRIPT_DIR/app/build_and_run.sh"
