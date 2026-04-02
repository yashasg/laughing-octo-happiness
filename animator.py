import tkinter as tk

from config import CopilotStatus, FRAME_DELAY, CONTEXT_MAX_BYTES
from sprite import SpriteRenderer
from overlay import OverlayWindow


class Animator:
    """Drives frame-by-frame animation, coordinating sprite and overlay."""

    def __init__(self, root: tk.Tk, sprite_renderer: SpriteRenderer, overlay: OverlayWindow):
        self._root = root
        self._sprite = sprite_renderer
        self._overlay = overlay
        self._current_status = CopilotStatus.IDLE
        self._frame_index = 0
        self._status_text = ""
        self._model_name = ""
        self._context_bytes = 0
        self._running = False
        self._after_id = None
        self._monitor = None  # set by main after creation

    def set_status(self, status: CopilotStatus, status_text: str = ""):
        """Called from StatusMonitor (possibly from a background thread).
        Must be thread-safe — use root.after() to schedule on main thread."""
        self._root.after(0, self._apply_status, status, status_text)

    def _apply_status(self, status: CopilotStatus, status_text: str):
        """Apply a status change on the main tkinter thread."""
        if status == self._current_status and status_text == self._status_text:
            return

        self._current_status = status
        self._status_text = status_text
        self._frame_index = 0
        self._overlay.set_status_text(status_text or status.value.title())

    def start(self):
        """Begin the animation loop."""
        self._running = True
        self._tick()

    def stop(self):
        """Stop the animation loop."""
        self._running = False
        if self._after_id is not None:
            self._root.after_cancel(self._after_id)
            self._after_id = None

    def _tick(self):
        """Single animation frame."""
        if not self._running:
            return

        self._frame_index += 1

        # Pull model/context info from monitor if available
        model_name = ""
        context_ratio = 0.0
        if self._monitor is not None:
            model_name = self._monitor.model_name
            ctx_bytes = self._monitor.context_bytes
            context_ratio = min(1.0, ctx_bytes / CONTEXT_MAX_BYTES) if CONTEXT_MAX_BYTES > 0 else 0.0

        # Get frame from sprite renderer
        photo, x_off, y_off = self._sprite.get_frame(
            self._current_status,
            self._frame_index,
            status_text=self._status_text,
            model_name=model_name,
            context_ratio=context_ratio,
        )

        # Update overlay
        self._overlay.update_sprite(photo, x_off, y_off)

        # Re-assert always-on-top every ~5 s (90 frames @ 18 fps)
        if self._frame_index % 90 == 0:
            self._overlay.maintain_topmost()

        # Schedule next frame
        self._after_id = self._root.after(FRAME_DELAY, self._tick)
