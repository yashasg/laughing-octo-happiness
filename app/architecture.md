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

---
---

# VS Code Extension Milestone — Architecture Review

> Reviewed: 2026-04-06
> Reviewer: Copilot (System Architecture Reviewer)
> Scope: Issues #9–#20 (vscode-extension milestone)
> System Type: Hybrid — TypeScript VS Code extension + native C++17/raylib binary

---

## 1. Milestone Overview

The vscode-extension milestone wraps the existing Copilot Buddy C++ overlay in a VS Code extension for single-click Marketplace distribution. The extension spawns, manages, and terminates the native binary, adds IDE integration (status bar, settings, auth delegation), and packages everything as a VSIX.

### Dependency Graph

```
#9  Extension scaffold ←──────────┬───────────────────────┐
    │                              │                       │
    ▼                              ▼                       │
#10 Binary spawning            #14 VSIX pipeline ◄── #13 macOS CI
    │                              │
    ├──► #11 Status bar            ▼
    ├──► #12 Settings          #15 Bundle resources
    └──► #18 Auth delegation       │
                                   ▼
                               #17 Resource path resolution
#16 SIGTERM handler (independent, C++ side)

#19 Integration tests (depends on #9, #10, #11, #12)
#20 Manual test plan (depends on all)
```

### Critical Path

`#9 → #10 → #11/#12/#18` (extension core)
`#13 → #14 → #15 → #17` (build & packaging)

These two tracks can proceed in parallel once #9 is done.

---

## 2. Architecture Decision Records

### ADR-006: Native Binary + VS Code Extension Hybrid Architecture

**Status:** Proposed | **Relates to:** #9, #10, #14, #15, #17

**Context:** The rendering engine (raylib, spritesheets, 30 FPS animation) cannot run inside a VS Code webview. The extension must manage the native binary as a child process.

**Options considered:**

| Option | Pros | Cons |
|--------|------|------|
| **A: Hybrid (CHOSEN)** — Extension spawns native binary | Reuses existing engine; no performance compromise; clear separation | 4-platform binary matrix; VSIX size ~20–40 MB; process mgmt complexity |
| **B: Pure webview** — Canvas/WebGL rewrite | Single stack (TS); small VSIX | Full rewrite; no floating overlay UX; webview perf limitations |
| **C: LSP bridge** — JSON-RPC between extension and binary | Clean IPC protocol | Over-engineered for one-way display; still needs binary distribution |

**Decision:** Option A. The extension spawns and manages the native binary.

**Risks:**
- macOS Gatekeeper may quarantine unsigned binaries. Mitigate with code signing or `xattr -cr` in activation.
- Windows AV may flag unknown spawned binaries. Mitigate with trusted code signing.
- VS Code Marketplace may scrutinize extensions spawning native processes.

---

### ADR-007: Cross-Platform VSIX Packaging Strategy

**Status:** Proposed | **Relates to:** #13, #14, #15

**Options considered:**

| Option | VSIX Size | Complexity | UX |
|--------|-----------|------------|-----|
| **A: Universal VSIX** — all binaries in one package | ~20–40 MB | Low | Everyone downloads everything |
| **B: Per-platform VSIXes (RECOMMENDED)** — `vsce --target` | ~5–10 MB each | Medium | Marketplace auto-selects |
| **C: Download on activation** — thin VSIX, fetch binary | ~100 KB | High | Requires internet; fragile first-run |

**Recommendation:** Option B for release; Option A acceptable as MVP.

**Platform matrix:**

| Target | Binary | CI Runner |
|--------|--------|-----------|
| `darwin-arm64` | `bin/darwin-arm64/copilot-buddy` | macOS Apple Silicon |
| `darwin-x64` | `bin/darwin-x64/copilot-buddy` | macOS Intel / cross-compile |
| `win32-x64` | `bin/win32-x64/copilot-buddy.exe` | Windows |
| `linux-x64` | `bin/linux-x64/copilot-buddy` | Linux |

**Open questions:**
1. Support Linux arm64? (dev containers, Raspberry Pi)
2. Support Windows arm64? (Surface Pro, Snapdragon)
3. Code signing strategy for macOS? (Ad-hoc, Developer ID, none?)

---

### ADR-008: Auth Token Delegation

**Status:** Proposed | **Relates to:** #18

**Decision:** Pass the VS Code GitHub auth token as `COPILOT_GITHUB_TOKEN` env var when spawning the binary. The C++ `ensure_github_token()` already checks this env var first — no C++ changes needed.

```typescript
const session = await vscode.authentication.getSession('github', ['read:user'], { createIfNone: true });
child_process.spawn(binaryPath, args, { env: { ...process.env, COPILOT_GITHUB_TOKEN: session.accessToken } });
```

**Security considerations:**
- Token is visible in `/proc/<pid>/environ` — standard practice, same as `gh` CLI
- Extension must never log the token value
- Token cannot be refreshed without restarting the binary — consider monitoring for expiry

**Alternatives rejected:** Stdin pipe (requires C++ changes), temp file (leakage risk), IPC channel (over-engineered).

---

### ADR-009: Process Lifecycle Management

**Status:** Proposed | **Relates to:** #10, #16

**Decision:** The extension manages a single native binary instance with the following lifecycle:

```
activate() → spawn binary (if autoStart) → track PID
    │
    ├─ on unexpected exit → show notification → offer restart
    ├─ on user "Stop" command → kill process → update status bar
    ├─ on user "Start" command (if stopped) → spawn again
    └─ deactivate() → SIGTERM (Unix) / TerminateProcess (Windows)
```

**Key design decisions:**
1. **Detached spawn** — `child_process.spawn` with `detached: false` and `stdio: 'ignore'`. NOT truly detached — we WANT the child to die when VS Code exits.
2. **Single instance** — guard against double-spawn. Track PID; reject Start if already running.
3. **Graceful shutdown** — send SIGTERM (Unix) first, then SIGKILL after 5s timeout. On Windows, use `TerminateProcess`.
4. **Crash recovery** — on unexpected exit, show info notification with "Restart" action. Don't auto-restart (avoids crash loops).
5. **chmod +x** — on macOS/Linux, set executable permission after VSIX extraction (`fs.chmodSync(binaryPath, 0o755)`).

**Open questions:**
1. Should crash count be tracked? (e.g., stop offering restart after 3 crashes in 5 minutes)
2. Should the binary's stdout/stderr be captured and shown in an output channel for debugging?

---

### ADR-010: Resource Bundling and Path Resolution

**Status:** Proposed | **Relates to:** #15, #17

**Context:** The C++ binary loads sprites and fonts via `GetApplicationDirectory() + "resources/"`. When the binary is launched from a VSIX extraction path, `GetApplicationDirectory()` may not point where expected.

**Decision:**
1. Bundle `app/resources/` alongside each platform binary: `bin/{platform}/resources/`
2. Add a `--resources-dir <path>` CLI argument as fallback
3. Extension passes `--resources-dir` pointing to the VSIX extraction path when spawning

**Directory layout inside VSIX:**
```
bin/
  darwin-arm64/
    copilot-buddy
    resources/
      IDLE.png
      RUN.png
      fonts/
        PixelifySans-Regular.ttf
  win32-x64/
    copilot-buddy.exe
    resources/
      ...
```

**Risk:** `GetApplicationDirectory()` in raylib returns the directory of the running executable, which should be correct if resources are co-located. But VSIX extraction paths can be deeply nested and vary by OS. The `--resources-dir` fallback is essential insurance.

---

## 3. Issue-by-Issue Architecture Comments

### #9 — Extension Scaffold

**Architecture notes:**
- Activation event: `onStartupFinished` is correct for autoStart. But also register `onCommand:copilot-buddy.start` so the extension activates on-demand even with `autoStart=false`.
- Use VS Code's `ExtensionContext.globalStorageUri` for any state the extension needs to persist (e.g., last-known binary PID for orphan cleanup).
- **Agent concern:** This is a TypeScript task, but assigned to "Architect → C++ Expert (or general-purpose for TypeScript)." A dedicated **TypeScript/VS Code Extension** agent should handle this — see Section 5.

### #10 — Binary Spawning & Process Lifecycle

**Architecture notes:**
- The platform detection map is well-defined. One concern: `process.arch` returns `arm64` on Apple Silicon, but in Rosetta 2 it returns `x64`. The extension should detect this correctly or the wrong binary could be launched.
- **Orphan prevention:** If VS Code crashes without calling `deactivate()`, the overlay binary becomes orphaned. Consider writing the PID to `ExtensionContext.globalStorageUri` and killing stale PIDs on next activation.
- **Detached vs attached:** Issue says "Spawn binary as detached child process." I'd recommend NOT fully detaching (no `detached: true` with `unref()`). Instead, keep a reference so `deactivate()` can reliably terminate it.
- `chmod +x` after VSIX extraction is essential — VSIX is a zip file and doesn't preserve Unix permissions.

### #11 — Status Bar Integration

**Architecture notes:**
- Currently the extension has no way to know the binary's internal status (IDLE/BUSY/WAITING). The status bar can only show Running/Stopped.
- **Future consideration:** To show richer status (e.g., "Copilot is thinking…" in the status bar), the binary would need to report its status back to the extension — via stdout, a status file, or an IPC channel. This is NOT needed for MVP but should be designed for.
- Keep the status bar item in the right side (`vscode.StatusBarAlignment.Right`) — left side is conventionally for language/branch info.

### #12 — Extension Settings

**Architecture notes:**
- `copilotBuddy.autoStart` and `copilotBuddy.verbose` are a good starting set.
- Consider adding `copilotBuddy.binaryPath` (string, optional) — allows users to point to a custom-built binary for development. This helps contributors test C++ changes without rebuilding the VSIX.
- Settings should be validated before use. If `verbose` is true, pass `--verbose` to the binary AND enable an Output Channel in VS Code for extension-side logging.

### #13 — macOS CI Workflow

**Architecture notes:**
- Apple Silicon (arm64) requires either a native `macos-14` runner (GitHub provides these) or cross-compilation.
- Cross-compiling C++ for arm64 on an x64 runner with raylib may be problematic due to raylib's platform-specific build. Native runners are recommended.
- The CI workflow should also produce a universal binary (`lipo -create`) as an optimization, though per-platform VSIXes (ADR-007) make this less critical.
- **Agent concern:** This is a CI/DevOps task. Assigning it to "Architect → C++ Expert" is reasonable but a **CI/DevOps** agent would be more appropriate — see Section 5.

### #14 — VSIX Build Pipeline

**Architecture notes:**
- This is the most complex CI workflow in the milestone. It needs to:
  1. Fan out: build binaries on 4 platforms (matrix)
  2. Fan in: collect all binaries into one workspace
  3. Package: run `vsce package` (or `vsce package --target <platform>` for per-platform VSIXes)
- Consider using a GitHub Actions `workflow_call` or a separate release workflow that triggers after all CI checks pass, rather than running on every push.
- **VSIX artifact versioning:** The VSIX version should come from `package.json` and match the C++ `COPILOT_BUDDY_VERSION`. Keep these in sync — perhaps generate both from a single source (e.g., `VERSION` file or git tag).

### #15 — Bundle Resources

**Architecture notes:**
- Resource duplication: each platform binary gets its own copy of `resources/`. For a VSIX, this means 4× the sprite/font files. At ~200 KB of resources, this is ~800 KB total — acceptable.
- If VSIX size becomes a concern later, resources could be shared at the extension root (`resources/`) and the binary told where to find them via `--resources-dir`.

### #16 — SIGTERM Handler (Graceful Shutdown)

**Architecture notes:**
- This is a critical reliability item for the hybrid architecture. Without it, the binary may leave dmon watchers running or fail to call `CloseWindow()`.
- On Windows, `CTRL_CLOSE_EVENT` fires when the console window is closed, but since the binary is spawned without a console (stdio: 'ignore'), this may not fire. `TerminateProcess` from the extension side is the realistic shutdown path on Windows.
- Consider also handling `SIGINT` for Ctrl+C during development.
- The handler should set an atomic flag (`std::atomic<bool> g_should_quit`) checked in the main loop, NOT call raylib functions directly from the signal handler (not signal-safe).

### #17 — Resource Path Resolution

**Architecture notes:**
- This issue correctly identifies the fragility of `GetApplicationDirectory()` in VSIX context.
- The extension should always pass `--resources-dir` explicitly — don't rely on `GetApplicationDirectory()` being correct. Defense in depth.
- Test with deeply nested extraction paths (VS Code extensions extract to `~/.vscode/extensions/<publisher>.<name>-<version>/`).

### #18 — Auth Token Passing

**Architecture notes:**
- See ADR-008 above for full analysis.
- One additional concern: `createIfNone: true` will show a login prompt to the user if not authenticated. For a background overlay, this might be surprising. Consider `createIfNone: false` with a graceful fallback to `gh` CLI, and only prompt explicitly when the user invokes a command.
- The `read:user` scope may not be sufficient for the Copilot API. Test this empirically.

### #19 — Integration Tests

**Architecture notes:**
- `@vscode/test-electron` is the right framework choice.
- The test for "binary spawns" needs a stub binary — you can't run the real raylib binary in CI (no display). Create a simple test binary that exits immediately or a mock.
- Consider using `sinon` or similar for mocking `child_process.spawn` in unit tests, and reserve integration tests for the full lifecycle.
- Test the chmod +x codepath on Linux CI runners.
- **Agent concern:** Integration tests for a TypeScript VS Code extension are outside the C++ Expert's domain. A **TypeScript/VS Code Extension** agent or the **Test Engineer** should own this.

### #20 — Manual Test Plan

**Architecture notes:**
- The test plan is comprehensive. Two additions:
  1. **Cross-process cleanup:** Close VS Code via Task Manager / `kill -9` (not graceful close) and verify the overlay process terminates.
  2. **Multi-window:** Open two VS Code windows. Only one overlay should exist (or should each window get its own? This is an open UX question).
- Consider automating some of these scenarios with `@vscode/test-electron` to reduce manual regression burden.

---

## 4. Cross-Cutting Concerns

### 4.1 Two-Stack Maintenance

The project now spans two technology stacks:
- **C++ (app/):** raylib rendering, status monitoring, file watching
- **TypeScript (vscode-extension/):** VS Code API, process management, settings

**Recommendation:** Document the boundary clearly. The extension should be as thin as possible — a "launcher" that delegates all intelligence to the binary. Avoid duplicating logic (e.g., don't parse events.jsonl in TypeScript).

### 4.2 Version Synchronization

The binary version (`COPILOT_BUDDY_VERSION` in CMakeLists.txt) and the extension version (`package.json` version) must stay in sync. If they diverge, users may run a binary incompatible with their extension.

**Recommendation:** Use a single `VERSION` file at the repo root. Both CMake and the extension's `package.json` read from it.

### 4.3 Update Mechanism

The milestone doesn't address how users update the binary when a new version ships.

**Open question:** VS Code auto-updates extensions from the Marketplace. When the VSIX updates, does the old binary get replaced? The answer depends on whether VS Code overwrites the extension directory entirely (it does). But the running binary must be terminated before the update replaces it.

### 4.4 Telemetry / Error Reporting

No issue covers telemetry or error reporting. For alpha, this is acceptable. For GA, consider:
- Extension-side error logging to an Output Channel
- Opt-in crash reporting for the binary

### 4.5 The Outdated `.github/copilot-instructions.md`

As noted in the Phase 1 architecture review, `.github/copilot-instructions.md` still describes the app as Python/tkinter/Pillow. This is actively misleading agents. **This should be updated before the milestone begins** — agents working on the extension will read these instructions and make wrong assumptions.

---

## 5. Missing Agents

The current agent roster:
- `expert-cpp-software-engineer.agent.md` — C++ implementation
- `pr-reviewer.agent.md` — Code review (read-only)
- `se-system-architecture-reviewer.agent.md` — Architecture review
- `test-engineer.agent.md` — GoogleTest gap analysis

**Gaps identified for this milestone:**

| Missing Agent | Needed For | Why |
|---------------|-----------|-----|
| **TypeScript / VS Code Extension Specialist** | #9, #11, #12, #18, #19 | Most extension issues are TypeScript work. The C++ Expert agent shouldn't be writing TypeScript VS Code extensions. Need an agent fluent in the VS Code API, `@vscode/test-electron`, `vsce`, and TypeScript project structure. |
| **CI/DevOps Engineer** | #13, #14 | Building a cross-platform matrix CI pipeline with VSIX packaging is specialized DevOps work. Neither the C++ Expert nor the Architect is the right fit. |
| **Security Reviewer** | #18 | Auth token handling deserves a focused security review. The PR reviewer catches code issues but may not catch security-specific concerns like token leakage, scope escalation, or timing attacks. |

**Recommendation:** Create at minimum a `vscode-extension-engineer.agent.md` before starting the milestone. The CI/DevOps and Security gaps can be covered by expanding the existing agents' scope, but the TypeScript gap is critical — multiple issues depend on it.

---

## 6. Milestone Risk Assessment

| Risk | Severity | Likelihood | Mitigation |
|------|----------|-----------|------------|
| macOS Gatekeeper blocks binary | HIGH | HIGH | Code signing or documented workaround |
| VSIX too large for Marketplace | MEDIUM | LOW | Per-platform VSIXes (ADR-007) |
| Binary orphaned after VS Code crash | MEDIUM | MEDIUM | PID tracking + cleanup on activate |
| Resource paths break in VSIX context | HIGH | MEDIUM | Always pass `--resources-dir`; test on all platforms |
| Auth token expires during long session | LOW | MEDIUM | Monitor token validity; offer restart |
| No TypeScript agent available | HIGH | HIGH | Create `vscode-extension-engineer.agent.md` |
| `.github/copilot-instructions.md` misleads agents | MEDIUM | HIGH | Update instructions before milestone starts |

---

## 7. Recommended Execution Order

1. **Pre-work (before any issues):**
   - Update `.github/copilot-instructions.md` to reflect C++ reality
   - Create `vscode-extension-engineer.agent.md`
   
2. **Phase 1 — Foundation (parallel tracks):**
   - Track A: #9 (scaffold) → #10 (spawning) → #11, #12, #18 (parallel)
   - Track B: #13 (macOS CI) → #14 (VSIX pipeline) → #15 (bundle) → #17 (path resolution)
   - Track C: #16 (SIGTERM handler) — independent

3. **Phase 2 — Testing:**
   - #19 (integration tests)
   - #20 (manual test plan)

---

## 8. Summary

The vscode-extension milestone is well-structured with clear dependencies and focused issues. The hybrid architecture (native binary + extension launcher) is the right choice — it preserves the existing rendering quality while solving the distribution problem.

**Key risks to address before starting:**
1. Create a TypeScript/VS Code Extension agent (most issues need one)
2. Update the outdated copilot-instructions.md
3. Decide on the code signing strategy for macOS

**Key architectural decisions to finalize:**
1. Universal VSIX vs per-platform VSIXes (ADR-007)
2. Detached vs attached child process (ADR-009)
3. Auth token scope requirements (ADR-008)

The milestone can deliver a working alpha if the two parallel tracks (extension core + build pipeline) execute concurrently and converge at the testing phase.

---

*— Signed: Copilot (System Architecture Reviewer Agent), 2026-04-06*
