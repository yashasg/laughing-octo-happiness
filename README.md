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

- Python 3.10+
- Windows, macOS, or Linux
- [GitHub Copilot CLI](https://docs.github.com/en/copilot) installed and configured

## Setup

### Windows

```bat
cd L:\watcher
python -m venv .venv
.venv\Scripts\pip.exe install -r requirements.txt
.venv\Scripts\python.exe main.py
```

Or use the included batch file:

```bat
run.bat
```

### macOS / Linux

```bash
chmod +x run.sh
./run.sh
```

The script creates a virtual environment, installs dependencies, and launches the app.

## Running Tests

```bash
pip install -r requirements-dev.txt
python -m pytest tests/ -v
```

## Versioning

This project uses [semantic versioning](https://semver.org/). The current version is defined in `version.py`.

## Dependencies

| Package    | Purpose                              |
|------------|--------------------------------------|
| `Pillow`   | Spritesheet rendering & compositing  |
| `watchdog` | File system event monitoring         |
| `pystray`  | System tray icon                     |

## How It Works

Copilot Buddy monitors `~/.copilot/session-state/` for active sessions. It uses **watchdog** to watch for changes to `events.jsonl` files (with a polling fallback) and parses the tail of the event log to determine:

- **Status** — derived from event types like `assistant.turn_start` (busy), `assistant.turn_end` (waiting), `session.task_complete` (idle)
- **Intent text** — extracted from `report_intent` tool calls
- **Model name** — from `session.start` events
- **Context usage** — approximated by `events.jsonl` file size

The overlay renders animated sprites with a speech bubble and info bar using Pillow, displayed in a borderless transparent tkinter window.

## License

MIT
