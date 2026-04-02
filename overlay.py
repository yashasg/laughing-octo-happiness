import tkinter as tk
from typing import Callable, Optional

from PIL import ImageTk

from config import CANVAS_W, CANVAS_H, OVERLAY_DEFAULT_X, OVERLAY_DEFAULT_Y, TRANSPARENT_COLOR


class OverlayWindow:
    """Transparent, always-on-top, draggable desktop overlay."""

    on_quit: Optional[Callable] = None

    def __init__(self, root: tk.Tk):
        """Create the overlay window as a Toplevel."""
        self.window = tk.Toplevel(root)
        self.window.overrideredirect(True)
        self.window.attributes('-topmost', True)

        # Tk 9+ on macOS dropped -transparentcolor; use -transparent + systemTransparent instead.
        try:
            self.window.attributes('-transparentcolor', TRANSPARENT_COLOR)
            canvas_bg = TRANSPARENT_COLOR
        except tk.TclError:
            self.window.attributes('-transparent', True)
            self.window.configure(bg='systemTransparent')
            canvas_bg = 'systemTransparent'

        screen_w = root.winfo_screenwidth()
        screen_h = root.winfo_screenheight()
        x = screen_w + OVERLAY_DEFAULT_X - CANVAS_W
        y = screen_h + OVERLAY_DEFAULT_Y - CANVAS_H
        self.window.geometry(f"{CANVAS_W}x{CANVAS_H}+{x}+{y}")

        self.canvas = tk.Canvas(
            self.window, width=CANVAS_W, height=CANVAS_H,
            bg=canvas_bg, highlightthickness=0,
        )
        self.canvas.pack()

        self._center_x = CANVAS_W // 2
        self._center_y = CANVAS_H // 2
        self._image_item = self.canvas.create_image(
            self._center_x, self._center_y, anchor='center',
        )
        self._current_image = None

        self._status_text = "Idle"
        self.on_quit = None

        self._setup_drag()
        self._setup_context_menu()

    # ------------------------------------------------------------------

    def update_sprite(
        self,
        photo_image: ImageTk.PhotoImage,
        x_offset: int = 0,
        y_offset: int = 0,
    ):
        """Update the displayed sprite on the canvas."""
        self._current_image = photo_image
        self.canvas.itemconfig(self._image_item, image=photo_image)
        self.canvas.coords(
            self._image_item,
            self._center_x + x_offset,
            self._center_y + y_offset,
        )

    def set_status_text(self, status_text: str):
        """Update the status label shown in the context menu."""
        self._status_text = status_text
        self._menu.entryconfig(self._status_menu_index, label=f"Status: {status_text}")

    def maintain_topmost(self):
        """Re-assert always-on-top so macOS can't push the window behind others."""
        self.window.attributes('-topmost', True)
        self.window.lift()

    # ------------------------------------------------------------------

    def _setup_drag(self):
        """Set up drag-to-move functionality."""

        def on_press(event):
            self._drag_x = event.x
            self._drag_y = event.y

        def on_drag(event):
            dx = event.x - self._drag_x
            dy = event.y - self._drag_y
            x = self.window.winfo_x() + dx
            y = self.window.winfo_y() + dy
            self.window.geometry(f"+{x}+{y}")

        self.canvas.bind('<Button-1>', on_press)
        self.canvas.bind('<B1-Motion>', on_drag)

    def _setup_context_menu(self):
        """Set up right-click context menu."""
        self._menu = tk.Menu(self.window, tearoff=0)
        self._menu.add_command(label="Status: Idle", state='disabled')
        self._status_menu_index = 0
        self._menu.add_separator()
        self._menu.add_command(label="Quit", command=self._on_quit_click)

        def on_right_click(event):
            self._menu.tk_popup(event.x_root, event.y_root)

        self.canvas.bind('<Button-3>', on_right_click)

    def _on_quit_click(self):
        if self.on_quit is not None:
            self.on_quit()
