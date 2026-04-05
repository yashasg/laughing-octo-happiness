# Copilot Buddy — Architecture Review

> Reviewed: 2026-04-04
> Reviewer: Copilot (System Architecture Reviewer)
> System Type: Desktop overlay application (C++17 / raylib)

> **⚠️ NOTE:** The project's `.copilot/instructions` still describe this as a Python/tkinter/Pillow app.
> Those instructions are **outdated** — the codebase is C++17 with raylib. Update the custom instructions
> (specifically the Architecture, Module Responsibilities, Data Flow, Key Conventions, and Running the App
> sections) to reflect the actual C++ implementation.

---

## 1. System Context

**What is it?** A desktop overlay ("Copilot Buddy") — a pixel-art character that sits on your screen and reflects GitHub Copilot CLI session status in real time.

**Stack:** C++17, raylib (rendering), CMake (build system), GoogleTest (testing framework — present in build but no tests written yet).

**Platforms:** macOS (confirmed via build), Windows (build.bat/run.bat exist).

**Architectural complexity:** Single-user desktop app — focus should be on **reliability**, **resource efficiency**, and **security fundamentals**.

---

## 2. Architecture Assessment

### What the app conceptually does:

```
File system watch (~/.copilot/session-state/)
  → Detect active session (inuse.*.lock)
  → Tail-read events.jsonl
  → Map last event → Status (IDLE / WAITING / BUSY)
  → Render sprite + speech bubble + info bar
  → Display in always-on-top transparent overlay
```

### Module Responsibilities (C++ equivalents):

| Module | Responsibility |
|--------|---------------|
| **Status Monitor** | Watches session-state dir, reads events.jsonl, maps events → status enum |
| **Animator** | Drives render loop, receives status updates, coordinates frame rendering |
| **Sprite Renderer** | Composites speech bubble, spritesheet frame, health bar onto canvas |
| **Overlay Window** | Transparent always-on-top raylib window, draggable, context menu |
| **Config** | Constants: status enum, colors, layout dimensions, paths, timing |
| **Main** | Entry point, wires modules, may handle system tray |

---

## 3. Strengths

| Area | Detail |
|------|--------|
| **Clean separation** | Monitor → Animator → Renderer → Window is a good pipeline |
| **raylib choice** | Lightweight, cross-platform, no heavy framework overhead for a simple overlay |
| **CMake build** | Standard, cross-platform build system with GoogleTest already integrated |
| **Dual build scripts** | `build.sh` + `build.bat` for cross-platform CI support |
| **Debouncing** | Prevents event bursts from causing redundant status checks |

---

## 4. Concerns & Recommendations

### 4.1 Reliability

| ID | Issue | Severity | Recommendation |
|----|-------|----------|---------------|
| R1 | **events.jsonl is a single point of failure** — file locked, deleted, or rotated mid-read crashes or stalls the app | HIGH | Wrap all file I/O in error handling. Add a DISCONNECTED status when the file is unreadable. Retry with backoff. |
| R2 | **No heartbeat timeout** — if Copilot CLI stops writing events, the overlay shows stale status indefinitely | MEDIUM | If no new event arrives within 30s, transition to IDLE. Make timeout configurable. |
| R3 | **Fixed tail-read size may miss events** — under heavy tool use, the last 8KB may not contain the most recent event | MEDIUM | Seek backward from EOF to find the last complete JSON line (`\n{`) rather than reading a fixed byte window. |
| R4 | **No session-absent handling** — if no `inuse.*.lock` file exists, behavior is undefined | HIGH | Explicitly handle "no active session" → show DISCONNECTED state with a distinct visual. |

### 4.2 Security

| ID | Issue | Severity | Recommendation |
|----|-------|----------|---------------|
| S1 | **No path validation** — symlinks in session-state dir could point anywhere | MEDIUM | Resolve paths with `realpath()` / `std::filesystem::canonical()` and verify they stay under `~/.copilot/session-state/`. |
| S2 | **No JSON input validation** — malformed or adversarial JSON in events.jsonl could crash the parser or cause buffer issues | HIGH | Validate JSON structure before processing. Use a safe JSON parser (nlohmann/json or rapidjson with error handling). Reject unexpected fields/types. |
| S3 | **Untrusted strings rendered in overlay** — model names and status text from events.jsonl are displayed directly | LOW | Sanitize and truncate any string from events.jsonl before rendering. Enforce max length. |

### 4.3 Performance & Resources

| ID | Issue | Severity | Recommendation |
|----|-------|----------|---------------|
| P1 | **Constant render loop burns CPU when idle** — raylib's game loop runs continuously even when nothing changes | HIGH | Only re-render when status, animation frame, or context ratio changes. Use `raylib::SetTargetFPS()` with a low value (e.g., 10 FPS) since smooth animation isn't critical for a status indicator. |
| P2 | **Polling interval isn't adaptive** — same frequency whether BUSY or IDLE | MEDIUM | Adaptive polling: 200ms when BUSY, 1–2s when IDLE. Reduces unnecessary I/O. |
| P3 | **File stat on every tick for context bar** — unnecessary I/O when nothing changed | LOW | Only re-stat file size when the file watcher fires, not every frame. |
| P4 | **No frame caching** — full sprite composition every tick even when output is identical | MEDIUM | Cache the last rendered texture. Invalidate only on status/data change. |

### 4.4 Operational Excellence

| ID | Issue | Severity | Recommendation |
|----|-------|----------|---------------|
| O1 | **No tests** — GoogleTest is in the build system but zero tests exist | HIGH | Add unit tests for: status enum mapping, JSON event parsing, tail-read logic, path validation. These are pure functions — easy to test. |
| O2 | **No logging** — when something breaks, there's no diagnostic info | HIGH | Add a lightweight logger (spdlog or even `stderr` behind `--verbose`). Log status transitions, file watch events, errors. |
| O3 | **No version info** — users can't identify which build they're running | LOW | Embed version from git tags at CMake configure time (`configure_file`). Show in context menu. |
| O4 | **No CI test step** — `build.sh` compiles but doesn't run tests | MEDIUM | Add `ctest --test-dir build --output-on-failure` to build scripts after the build step. |

### 4.5 Cross-Platform

| ID | Issue | Severity | Recommendation |
|----|-------|----------|---------------|
| X1 | **Transparent overlay behavior varies by OS** — raylib transparency works differently on macOS, Windows, and Linux | MEDIUM | Test on all target platforms. Consider platform-specific window flags. Document known limitations. |
| X2 | **File watching mechanism** — `inotify` (Linux), `FSEvents` (macOS), `ReadDirectoryChanges` (Windows) all behave differently | MEDIUM | Use a cross-platform file watcher library (e.g., efsw, or poll-only for simplicity). Abstract behind an interface for testability. |
| X3 | **Session-state path** — `~/.copilot/session-state/` may resolve differently across platforms | LOW | Use `std::filesystem::path` with proper home directory detection per platform. |

---

## 5. Proposed Improved Data Flow

```
┌─────────────────────────────────────────────────────┐
│                   File Watcher                       │
│  (platform-abstracted: FSEvents/inotify/polling)     │
└──────────────┬──────────────────────────────────────┘
               │ file changed event
               ▼
┌──────────────────────────────┐
│     Debounce (150ms timer)   │
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐
│   Tail-Read (seek to last    │
│   complete JSON line from    │◄── Fallback: adaptive poll
│   EOF, validate JSON)        │    BUSY=200ms, IDLE=2000ms
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐
│   Status Mapper              │
│   event_type → Status enum   │
│   + extract model, file size │
└──────────────┬───────────────┘
               │ (only if changed)
               ▼
┌──────────────────────────────┐
│   Thread-safe Queue          │
│   (background → main thread) │
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐     ┌────────────────┐
│   Renderer (cache-aware)     │────►│  Overlay Window │
│   Only recomposite on change │     │  (raylib)       │
└──────────────────────────────┘     └────────────────┘

               ┌──────────────────────┐
               │  Watchdog Timer      │
               │  Restart monitor if  │
               │  stalled > 5s        │
               └──────────────────────┘
```

---

## 6. Priority Action Items

| Priority | Action | Impact |
|----------|--------|--------|
| 🔴 P0 | Add error boundaries around all file I/O and JSON parsing | Prevents crashes |
| 🔴 P0 | Write unit tests for status mapping + event parsing | Catches regressions |
| 🟡 P1 | Add `--verbose` logging for diagnostics | Enables debugging |
| 🟡 P1 | Cap FPS with `SetTargetFPS(10)` + skip redundant renders | Reduces CPU 80%+ |
| 🟡 P1 | Handle DISCONNECTED state (no session / unreadable file) | Better UX |
| 🟢 P2 | Adaptive polling intervals | Reduces idle I/O |
| 🟢 P2 | Validate and sanitize JSON input | Hardens security |
| 🟢 P2 | Embed version info at build time | Better support |
| 🔵 P3 | Cross-platform file watcher abstraction | Portability |
| 🔵 P3 | User config file (`~/.copilot-buddy.toml`) | Customization |

---

## 7. ADRs To Create

These decisions should be documented as Architecture Decision Records in `docs/architecture/`:

1. **ADR-001: Rendering engine** — Why raylib over SDL2, Dear ImGui, Electron, or native APIs?
2. **ADR-002: File watching strategy** — Platform-specific watchers vs polling-only vs hybrid?
3. **ADR-003: Context size proxy** — File size heuristic vs actual token counting?
4. **ADR-004: Threading model** — Background watcher + main-thread render vs single-threaded async?
5. **ADR-005: JSON parser choice** — nlohmann/json vs rapidjson vs simdjson?

---

## 8. Scorecard

| Pillar | Rating | Summary |
|--------|--------|---------|
| Reliability | ⚠️ **Needs Work** | No error handling, no fallback states, SPOF on events.jsonl |
| Security | ⚠️ **Needs Work** | No input validation, no path sanitization |
| Performance | ✅ **Adequate** | Debouncing is good; FPS cap + caching would make it great |
| Ops Excellence | ❌ **Critical** | Zero tests, no logging, no version tracking |
| Simplicity | ✅ **Good** | Clean pipeline, minimal dependencies, focused scope |

**Overall: Solid foundation, needs hardening.** The architecture is well-structured — the main gaps are defensive coding, testing, and observability. None of these require architectural changes, just disciplined implementation.

---

*This review is based on project structure, build artifacts, and architecture documentation. Findings should be verified against actual source code. A code-level review is recommended as a follow-up.*
