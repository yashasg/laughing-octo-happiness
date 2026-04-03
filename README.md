# Copilot Buddy

A pixel-art desktop buddy that reflects your GitHub Copilot CLI session status in real time — like Clippy, but for Copilot.

<!-- TODO: Add screenshot -->

## Features

- 🔵 **Blue** (Idle) — no active Copilot session
- 🟢 **Green** (Waiting) — Copilot finished, your turn to respond
- 🔴 **Red** (Busy) — Copilot is processing
- Always-on-top transparent overlay with animated spritesheets
- Draggable character — place it anywhere on your screen
- Speech bubble showing current status or live intent text
- Model name and context usage health bar (green → yellow → red)
- System tray icon with status indicator
- Right-click context menu

## Requirements

- CMake 3.15+
- A C++17 compiler (clang++ on macOS, MSVC or MinGW on Windows, g++ on Linux)
- Windows, macOS, or Linux
- [GitHub Copilot CLI](https://docs.github.com/en/copilot) installed and configured

## Setup

### macOS / Linux

```bash
chmod +x run.sh
./run.sh
```

The script builds the app with CMake and launches it.

### Windows

```bat
run.bat
```

Builds with CMake (Release) and runs the executable.

## Versioning

This project uses [semantic versioning](https://semver.org/). The version is defined in `app/CMakeLists.txt`.

## Dependencies

| Library          | Purpose                                      |
|------------------|----------------------------------------------|
| `raylib 5.5`     | Window, rendering, spritesheet animation     |
| `nlohmann/json`  | Parsing `events.jsonl`                       |
| `dmon`           | File system watching (vendored single header)|

raylib is downloaded automatically by CMake at build time.

## How It Works

Copilot Buddy monitors `~/.copilot/session-state/` for active sessions. It uses **dmon** to watch for changes to `events.jsonl` files (with a polling fallback) and parses the tail of the event log to determine:

- **Status** — derived from event types like `assistant.turn_start` (busy), `assistant.turn_end` (waiting), `session.task_complete` (idle)
- **Intent text** — extracted from `report_intent` tool calls
- **Model name** — from `session.start` events
- **Context usage** — approximated by `events.jsonl` file size

The overlay renders animated sprites with a speech bubble and info bar using raylib, displayed in a borderless transparent window.

## License

MIT
