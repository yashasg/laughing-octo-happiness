import json
import threading
from pathlib import Path
from typing import Callable, Optional

from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler, FileSystemEvent

from config import CopilotStatus, COPILOT_STATE_DIR, POLL_INTERVAL

_STATUS_MAP = {
    "assistant.turn_start": CopilotStatus.BUSY,
    "tool.execution_start": CopilotStatus.BUSY,
    "assistant.message": CopilotStatus.BUSY,
    "hook.start": CopilotStatus.BUSY,
    "assistant.turn_end": CopilotStatus.WAITING,
    "session.task_complete": CopilotStatus.IDLE,
}


class _EventHandler(FileSystemEventHandler):
    """Watches for changes to .jsonl and .lock files."""

    def __init__(self, callback: Callable[[], None]):
        super().__init__()
        self._callback = callback

    def on_modified(self, event: FileSystemEvent) -> None:
        if not event.is_directory and self._is_relevant(event.src_path):
            self._callback()

    def on_created(self, event: FileSystemEvent) -> None:
        if not event.is_directory and self._is_relevant(event.src_path):
            self._callback()

    @staticmethod
    def _is_relevant(path: str) -> bool:
        return path.endswith(".jsonl") or path.endswith(".lock")


class StatusMonitor:
    """Monitors Copilot CLI session state to determine current status."""

    def __init__(self, on_status_change: Callable[[CopilotStatus, str], None]):
        self._on_status_change = on_status_change
        self._last_status: CopilotStatus = CopilotStatus.IDLE
        self._last_text: str = ""
        self._last_model_name: str = ""
        self._last_file_size: int = 0
        self._lock = threading.Lock()
        self._stop_event = threading.Event()
        self._observer: Optional[Observer] = None
        self._poll_thread: Optional[threading.Thread] = None
        self._debounce_timer: Optional[threading.Timer] = None

    # -- public API --

    def start(self) -> None:
        """Begin monitoring with watchdog + polling fallback."""
        self._stop_event.clear()

        # Initial status check
        self._check_and_notify()

        # Watchdog observer
        self._start_observer()

        # Polling fallback
        self._poll_thread = threading.Thread(
            target=self._poll_loop, daemon=True, name="status-poll"
        )
        self._poll_thread.start()

    def stop(self) -> None:
        """Stop monitoring and clean up."""
        self._stop_event.set()

        if self._debounce_timer is not None:
            self._debounce_timer.cancel()

        if self._observer is not None:
            try:
                self._observer.stop()
                self._observer.join(timeout=2)
            except Exception:
                pass
            self._observer = None

        if self._poll_thread is not None:
            self._poll_thread.join(timeout=3)
            self._poll_thread = None

    def get_current_status(self) -> tuple[CopilotStatus, str]:
        """Synchronously check and return current status and text."""
        with self._lock:
            session = self._find_active_session()
            if session is None:
                return (CopilotStatus.IDLE, "")
            return self._parse_status(session)

    @property
    def model_name(self) -> str:
        """Last detected model name (e.g. 'claude-opus-4.6')."""
        return self._last_model_name

    @property
    def context_bytes(self) -> int:
        """Size of current session's events.jsonl in bytes."""
        return self._last_file_size

    # -- internal --

    def _start_observer(self) -> None:
        if not COPILOT_STATE_DIR.exists():
            print(
                f"[StatusMonitor] State dir not found: {COPILOT_STATE_DIR} "
                "— relying on polling"
            )
            return
        try:
            self._observer = Observer()
            handler = _EventHandler(self._debounced_check)
            self._observer.schedule(handler, str(COPILOT_STATE_DIR), recursive=True)
            self._observer.daemon = True
            self._observer.start()
        except Exception as exc:
            print(f"[StatusMonitor] Failed to start watchdog: {exc}")
            self._observer = None

    def _debounced_check(self) -> None:
        """Schedule a status check after a short debounce window."""
        if self._debounce_timer is not None:
            self._debounce_timer.cancel()
        self._debounce_timer = threading.Timer(0.15, self._check_and_notify)
        self._debounce_timer.daemon = True
        self._debounce_timer.start()

    def _poll_loop(self) -> None:
        interval = POLL_INTERVAL / 1000.0
        while not self._stop_event.is_set():
            self._stop_event.wait(interval)
            if not self._stop_event.is_set():
                self._check_and_notify()

    def _check_and_notify(self) -> None:
        with self._lock:
            session = self._find_active_session()
            if session:
                new_status, new_text = self._parse_status(session)
            else:
                new_status = CopilotStatus.IDLE
                new_text = ""
                self._last_file_size = 0
            if new_status != self._last_status or new_text != self._last_text:
                self._last_status = new_status
                self._last_text = new_text
                try:
                    self._on_status_change(new_status, new_text)
                except Exception as exc:
                    print(f"[StatusMonitor] Callback error: {exc}")

    def _find_active_session(self) -> Optional[Path]:
        """Return the session dir with a lock file and the newest events.jsonl."""
        try:
            if not COPILOT_STATE_DIR.exists():
                return None
        except (OSError, PermissionError):
            return None

        best: Optional[Path] = None
        best_mtime: float = 0.0

        try:
            entries = list(COPILOT_STATE_DIR.iterdir())
        except (OSError, PermissionError):
            return None

        for entry in entries:
            try:
                if not entry.is_dir():
                    continue
                # Check for at least one inuse.*.lock file
                if not any(entry.glob("inuse.*.lock")):
                    continue
                events_file = entry / "events.jsonl"
                if not events_file.exists():
                    continue
                mtime = events_file.stat().st_mtime
                if mtime > best_mtime:
                    best_mtime = mtime
                    best = entry
            except (OSError, PermissionError):
                continue

        return best

    def _parse_status(self, session_dir: Path) -> tuple[CopilotStatus, str]:
        """Parse the tail of events.jsonl to determine status, intent text,
        model name, and file size (context usage proxy)."""
        events_file = session_dir / "events.jsonl"
        try:
            size = events_file.stat().st_size
            self._last_file_size = size
            if size == 0:
                return (CopilotStatus.IDLE, "")

            with open(events_file, "rb") as fh:
                seek_pos = max(0, size - 8192)
                fh.seek(seek_pos)
                tail = fh.read().decode("utf-8", errors="replace")

            lines = tail.splitlines()

            # If we seeked into the middle, the first line is likely partial
            if seek_pos > 0 and lines:
                lines = lines[1:]

            status = CopilotStatus.IDLE
            status_text = ""
            found_status = False

            for line in reversed(lines):
                line = line.strip()
                if not line:
                    continue
                try:
                    event = json.loads(line)
                except (json.JSONDecodeError, ValueError):
                    continue
                event_type = event.get("type", "")
                data = event.get("data", {})

                # Find status (first match walking backwards)
                if not found_status and event_type in _STATUS_MAP:
                    status = _STATUS_MAP[event_type]
                    found_status = True

                # Find latest intent text (may be before the status event)
                if not status_text:
                    if event_type == "tool.execution_start" and data.get("toolName") == "report_intent":
                        status_text = data.get("arguments", {}).get("intent", "")

                # Pick up model name from session.start or tool.execution_complete
                if not self._last_model_name:
                    if event_type == "session.start":
                        self._last_model_name = data.get("selectedModel", "")
                    elif event_type == "tool.execution_complete" and data.get("model"):
                        self._last_model_name = data.get("model", "")

                # Stop once we have everything
                if found_status and status_text and self._last_model_name:
                    break

            # If we still don't have a model name, do a forward scan of just
            # the first few lines (session.start is typically near the top)
            if not self._last_model_name:
                self._scan_for_model(events_file)

        except FileNotFoundError:
            pass
        except (PermissionError, OSError, UnicodeDecodeError) as exc:
            print(f"[StatusMonitor] Error reading {events_file}: {exc}")

        return (status, status_text)

    def _scan_for_model(self, events_file: Path) -> None:
        """Read the head of events.jsonl looking for session.start model info."""
        try:
            with open(events_file, "r", encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        event = json.loads(line)
                    except (json.JSONDecodeError, ValueError):
                        continue
                    if event.get("type") == "session.start":
                        model = event.get("data", {}).get("selectedModel", "")
                        if model:
                            self._last_model_name = model
                        return
                    # Only scan first 20 lines
                    if fh.tell() > 4096:
                        return
        except (OSError, PermissionError):
            pass
