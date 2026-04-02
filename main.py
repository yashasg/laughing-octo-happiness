"""Copilot Desktop Buddy — main entry point.

A cute pixel-art character that lives on your desktop and changes color
based on GitHub Copilot CLI status:
  Green  = Idle (no active session)
  Yellow = Waiting (Copilot finished, awaiting your input)
  Red    = Busy (Copilot is processing)
"""

import tkinter as tk
import threading
from config import CopilotStatus
from sprite import SpriteRenderer
from overlay import OverlayWindow
from animator import Animator
from status_monitor import StatusMonitor


def main():
    # Create root window (hidden — only the overlay Toplevel is visible)
    root = tk.Tk()
    root.withdraw()

    # Create components
    sprite_renderer = SpriteRenderer()
    overlay = OverlayWindow(root)
    animator = Animator(root, sprite_renderer, overlay)
    monitor = StatusMonitor(on_status_change=animator.set_status)
    animator._monitor = monitor  # let animator read model/context info

    # Set up system tray icon (in a separate thread)
    tray_icon = setup_tray(shutdown_callback=None, animator=animator)

    # Wire quit handler
    def shutdown():
        monitor.stop()
        animator.stop()
        if tray_icon is not None:
            tray_icon.stop()
        root.quit()
        root.destroy()

    overlay.on_quit = shutdown
    # Patch the tray shutdown reference now that shutdown is defined
    if tray_icon is not None:
        _patch_tray_quit(tray_icon, shutdown)

    # Start everything
    animator.start()
    monitor.start()

    # Run tkinter main loop
    try:
        root.mainloop()
    except KeyboardInterrupt:
        shutdown()


def _patch_tray_quit(icon, shutdown_callback):
    """Replace the tray menu with one that calls the real shutdown callback."""
    try:
        import pystray

        icon.menu = pystray.Menu(
            pystray.MenuItem("Copilot Buddy", None, enabled=False),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem("Quit", lambda: shutdown_callback()),
        )
    except Exception:
        pass


def setup_tray(shutdown_callback, animator):
    """Create a system tray icon using pystray. Returns the icon object.

    The tray icon:
    - Shows a small colored circle matching current status
    - Menu items: "Copilot Buddy" (label), separator, "Quit"
    - Runs in its own daemon thread (pystray has its own event loop)
    """
    try:
        import pystray
        from PIL import Image, ImageDraw

        def create_tray_image(color=(0, 200, 80)):
            """Generate a 64x64 tray icon — colored circle."""
            img = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
            draw = ImageDraw.Draw(img)
            draw.ellipse([8, 8, 56, 56], fill=color + (255,))
            return img

        # Build a placeholder menu; the real quit callback is patched in later
        quit_action = (lambda: shutdown_callback()) if shutdown_callback else (lambda: None)

        icon = pystray.Icon(
            "copilot-buddy",
            create_tray_image(),
            "Copilot Buddy",
            menu=pystray.Menu(
                pystray.MenuItem("Copilot Buddy", None, enabled=False),
                pystray.Menu.SEPARATOR,
                pystray.MenuItem("Quit", quit_action),
            ),
        )

        # Run pystray in a daemon thread so it doesn't block tkinter's mainloop
        tray_thread = threading.Thread(target=icon.run, daemon=True)
        tray_thread.start()

        return icon

    except Exception as e:
        print(f"System tray not available: {e}")
        return None


if __name__ == "__main__":
    main()
