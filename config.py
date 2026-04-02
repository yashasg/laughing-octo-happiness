from pathlib import Path
from enum import Enum


class CopilotStatus(Enum):
    IDLE = "idle"           # No active session
    WAITING = "waiting"     # Waiting for user input
    BUSY = "busy"           # Copilot is processing


# Speech bubble colors (border/accent)
BUBBLE_COLORS = {
    CopilotStatus.IDLE: (70, 130, 220),      # Blue (calm/sleeping)
    CopilotStatus.WAITING: (0, 200, 80),     # Green (your turn)
    CopilotStatus.BUSY: (220, 50, 50),       # Red (working)
}

# Default speech bubble text (overridden by live intent when BUSY)
DEFAULT_LABELS = {
    CopilotStatus.IDLE: "Zzz...",
    CopilotStatus.WAITING: "Your turn!",
    CopilotStatus.BUSY: "Working...",
}

# Paths
COPILOT_STATE_DIR = Path.home() / ".copilot" / "session-state"
SPRITES_DIR = Path(__file__).parent / "sprites"

# Spritesheet config
SPRITE_WIDTH = 96              # Frame width in spritesheet
SPRITE_HEIGHT = 40             # Frame height in spritesheet (trimmed)
SPRITE_DISPLAY_SCALE = 3       # Scale factor for display
SPRITE_DISPLAY_W = SPRITE_WIDTH * SPRITE_DISPLAY_SCALE    # 288
SPRITE_DISPLAY_H = SPRITE_HEIGHT * SPRITE_DISPLAY_SCALE   # 120
IDLE_FRAMES = 10           # Frames in IDLE.png
RUN_FRAMES = 16            # Frames in RUN.png
CANVAS_W = 300             # Canvas width (288px sprite + padding)
CANVAS_H = 246             # Canvas height (bubble + sprite + info bar)

# Animation
FRAME_RATE = 18            # Spritesheet animation FPS (1.5x speed)
FRAME_DELAY = 1000 // FRAME_RATE

# Overlay
OVERLAY_DEFAULT_X = -260   # Offset from right edge
OVERLAY_DEFAULT_Y = -320   # Offset from bottom edge
TRANSPARENT_COLOR = "#010101"

# Status monitor
POLL_INTERVAL = 2000  # ms fallback polling interval

# Model / context info bar (rendered below sprite)
FALLBACK_MODEL_NAME = "Unknown"
CONTEXT_MAX_BYTES = 2 * 1024 * 1024  # 2 MB — events.jsonl size treated as "full"
