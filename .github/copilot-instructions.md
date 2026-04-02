# Copilot Instructions

## Behavioral Rules

- **Plan first**: Enter plan mode for any non-trivial task (3+ steps or architectural decisions). If something goes sideways, stop and re-plan immediately.
- **Subagents**: Use subagents liberally to keep main context clean. Offload research, exploration, and parallel analysis. One task per subagent.
- **Self-improvement**: After any correction from the user, update `tasks/lessons.md` with the pattern and a rule to prevent recurrence. Review lessons at session start.
- **Verify before done**: Never mark a task complete without proving it works. Run tests, check logs, demonstrate correctness.
- **Demand elegance (balanced)**: For non-trivial changes, pause and consider a more elegant approach. Skip this for simple, obvious fixes.
- **Autonomous bug fixing**: When given a bug report, just fix it — point at logs, errors, failing tests, then resolve them without hand-holding.
- **Simplicity first**: Make every change as simple as possible. Find root causes. No temporary fixes. Senior developer standards.

## Task Management

1. Write plan to `tasks/todo.md` with checkable items
2. Check in before starting implementation
3. Mark items complete as you go
4. High-level summary at each step
5. Add review section to `tasks/todo.md`
6. Update `tasks/lessons.md` after corrections

## Running the App

```
# Install dependencies (once)
cd L:\watcher && python -m venv .venv && .venv\Scripts\pip install -r requirements.txt

# Run
run.bat
# or: .venv\Scripts\python.exe main.py
```

No tests, linter, or CI exist. Verify changes manually by running the app.

## Architecture

A desktop overlay ("Copilot Buddy") — a pixel-art character that sits on your screen and reflects GitHub Copilot CLI session status in real time. Built with Python, tkinter, and Pillow.

### Status Model

The app derives status by tail-reading `events.jsonl` files in `~/.copilot/session-state/`:

| Status | Bubble Color | Trigger Events |
|--------|-------------|----------------|
| IDLE | Blue | No active session / `session.task_complete` |
| WAITING | Green | `assistant.turn_end` |
| BUSY | Red | `assistant.turn_start` / `tool.execution_start` / `assistant.message` |

### Module Responsibilities

- **`status_monitor.py`** — Watches `~/.copilot/session-state/` using `watchdog` + polling fallback. Finds the active session (has `inuse.*.lock` file), tail-reads `events.jsonl` (last 8KB), and maps the most recent event type to a `CopilotStatus`. Also extracts the model name and context file size. Fires a callback on status change.
- **`animator.py`** — Drives the animation loop via `root.after()`. Receives status changes from the monitor and delegates frame rendering to `SpriteRenderer`, then updates the overlay. Computes `context_ratio` from file size for the health bar.
- **`sprite.py`** — Renders each frame: speech bubble (with status text and colored border), spritesheet frame (IDLE or RUN), and an info bar (model name + context health bar). All drawing is done with Pillow onto an RGBA canvas, returned as `ImageTk.PhotoImage`.
- **`overlay.py`** — Transparent, always-on-top, draggable tkinter `Toplevel` window with a right-click context menu.
- **`config.py`** — All shared constants: status enum, colors, layout dimensions, paths, timing.
- **`main.py`** — Wires everything together. Also sets up a `pystray` system tray icon in a daemon thread.

### Data Flow

```
watchdog/polling → StatusMonitor._check_and_notify()
    → callback: Animator.set_status(status, text)
        → root.after(0, _apply_status)  # thread-safe handoff to main thread
            → Animator._tick() loop
                → SpriteRenderer.get_frame() → Pillow RGBA canvas → PhotoImage
                → OverlayWindow.update_sprite()
```

## Key Conventions

- **Thread safety**: `StatusMonitor` runs on background threads (watchdog + polling). All tkinter updates go through `root.after(0, ...)` to marshal onto the main thread. Never call tkinter methods directly from a background thread.
- **Spritesheets**: Character frames are horizontal strips in `sprites/IDLE.png` and `sprites/RUN.png`. Frames are sliced at `SPRITE_WIDTH` intervals and scaled up 3× with nearest-neighbor.
- **Context bar color**: The health bar uses a green→yellow→red gradient based on `events.jsonl` file size relative to `CONTEXT_MAX_BYTES` (2 MB). This is a proxy for context window usage — it does **not** read actual token counts.
- **Debouncing**: File change events are debounced with a 150ms `threading.Timer` to avoid redundant status checks during event bursts.
- **Transparency**: The overlay uses tkinter's `-transparentcolor` attribute. The canvas background is set to `#010101` which becomes invisible. All sprite rendering must avoid this exact color.
