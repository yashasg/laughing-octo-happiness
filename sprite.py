"""Spritesheet-based sprite renderer with speech bubble."""

from PIL import Image, ImageDraw, ImageFont, ImageTk
from config import (
    CopilotStatus, BUBBLE_COLORS, DEFAULT_LABELS,
    SPRITE_WIDTH, SPRITE_HEIGHT, SPRITE_DISPLAY_W, SPRITE_DISPLAY_H,
    CANVAS_W, CANVAS_H, SPRITES_DIR,
    IDLE_FRAMES, RUN_FRAMES, FALLBACK_MODEL_NAME,
)

# ---- Canvas layout constants -------------------------------------------
_BUBBLE_TOP = 6
_BUBBLE_BOTTOM = 52
_BUBBLE_RADIUS = 9
_BUBBLE_HEIGHT = _BUBBLE_BOTTOM - _BUBBLE_TOP
_BUBBLE_MIN_WIDTH = 105
_BUBBLE_PAD_X = 18           # horizontal text padding inside bubble
_TRI_WIDTH = 10
_TRI_HEIGHT = 8
_TRI_TIP_Y = _BUBBLE_BOTTOM + _TRI_HEIGHT   # 60
_SPRITE_Y = 62               # sprite top edge on canvas
_MAX_TEXT_LEN = 25

# Info bar layout (below sprite)
_INFO_Y = _SPRITE_Y + SPRITE_DISPLAY_H + 4    # model text top
_BAR_LABEL_Y = _INFO_Y + 20                    # "Context" label top
_BAR_Y = _BAR_LABEL_Y + 14                     # health bar top
_BAR_HEIGHT = 10
_BAR_WIDTH = 180
_BAR_RADIUS = 5


def _load_font() -> ImageFont.ImageFont:
    """Load a small font for speech bubble text."""
    for name in ("arial", "Arial", "segoeui", "tahoma", "verdana"):
        try:
            return ImageFont.truetype(name, 15)
        except (OSError, IOError):
            continue
    try:
        return ImageFont.load_default(size=15)
    except TypeError:
        return ImageFont.load_default()


def _load_small_font() -> ImageFont.ImageFont:
    """Load a bold font for the model name label."""
    for name in ("arialbd", "Arial Bold", "segoeui", "tahomabd", "verdanab"):
        try:
            return ImageFont.truetype(name, 14)
        except (OSError, IOError):
            continue
    # Fallback: try regular fonts at the same size
    for name in ("arial", "Arial", "tahoma", "verdana"):
        try:
            return ImageFont.truetype(name, 14)
        except (OSError, IOError):
            continue
    try:
        return ImageFont.load_default(size=14)
    except TypeError:
        return ImageFont.load_default()


def _load_label_font() -> ImageFont.ImageFont:
    """Load a small font for the health bar label."""
    for name in ("arial", "Arial", "segoeui", "tahoma", "verdana"):
        try:
            return ImageFont.truetype(name, 10)
        except (OSError, IOError):
            continue
    try:
        return ImageFont.load_default(size=10)
    except TypeError:
        return ImageFont.load_default()


def _extract_frames(sheet: Image.Image, count: int) -> list:
    """Slice a horizontal spritesheet into individual RGBA frames, scaled up."""
    frames = []
    for i in range(count):
        frame = sheet.crop((i * SPRITE_WIDTH, 0, (i + 1) * SPRITE_WIDTH, SPRITE_HEIGHT))
        scaled = frame.convert("RGBA").resize(
            (SPRITE_DISPLAY_W, SPRITE_DISPLAY_H), Image.NEAREST
        )
        frames.append(scaled)
    return frames


# ---- Main renderer -------------------------------------------------------

class SpriteRenderer:
    """Renders spritesheet frames with a speech bubble overlay."""

    def __init__(self):
        idle_sheet = Image.open(SPRITES_DIR / "IDLE.png")
        run_sheet = Image.open(SPRITES_DIR / "RUN.png")
        self._idle_frames = _extract_frames(idle_sheet, IDLE_FRAMES)
        self._run_frames = _extract_frames(run_sheet, RUN_FRAMES)
        self._font = _load_font()
        self._small_font = _load_small_font()
        self._label_font = _load_label_font()
        self._last_photo = None  # prevent GC of PhotoImage

    def get_frame(self, status: CopilotStatus, frame_index: int,
                  status_text: str = None,
                  model_name: str = "",
                  context_ratio: float = 0.0) -> tuple:
        """Returns (ImageTk.PhotoImage, x_offset: int, y_offset: int).

        Args:
            status: Current status (determines which spritesheet).
            frame_index: Frame number (wraps based on sheet frame count).
            status_text: Text to show in speech bubble.
                         If None, uses DEFAULT_LABELS[status].
            model_name: Model name to display below the sprite.
            context_ratio: Context usage 0.0–1.0 for the health bar.
        """
        if status == CopilotStatus.BUSY:
            frames = self._run_frames
        else:
            frames = self._idle_frames

        frame = frames[frame_index % len(frames)]
        text = status_text if status_text else DEFAULT_LABELS[status]
        color = BUBBLE_COLORS[status]

        canvas = Image.new("RGBA", (CANVAS_W, CANVAS_H), (0, 0, 0, 0))

        # Speech bubble
        self._draw_bubble(canvas, text, color)

        # Sprite frame centered horizontally
        sprite_x = (CANVAS_W - SPRITE_DISPLAY_W) // 2
        canvas.paste(frame, (sprite_x, _SPRITE_Y), frame)

        # Model name + context health bar below sprite
        self._draw_info_bar(canvas, model_name, context_ratio)

        photo = ImageTk.PhotoImage(canvas)
        self._last_photo = photo
        return (photo, 0, 0)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _draw_bubble(self, canvas: Image.Image, text: str, color: tuple):
        """Draw a speech bubble with text onto the canvas."""
        draw = ImageDraw.Draw(canvas)
        cr, cg, cb = color[:3]

        # Truncate long text
        if len(text) > _MAX_TEXT_LEN:
            text = text[:_MAX_TEXT_LEN - 3] + "..."

        # Measure text
        try:
            bbox = draw.textbbox((0, 0), text, font=self._font)
            tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        except AttributeError:
            tw, th = draw.textsize(text, font=self._font)

        # Bubble dimensions
        bubble_w = max(_BUBBLE_MIN_WIDTH, tw + _BUBBLE_PAD_X * 2)
        bubble_left = (CANVAS_W - bubble_w) // 2
        bubble_right = bubble_left + bubble_w

        # Fill: white with 15% tint of bubble color
        fill = (
            int(255 * 0.85 + cr * 0.15),
            int(255 * 0.85 + cg * 0.15),
            int(255 * 0.85 + cb * 0.15),
            255,
        )
        border = (cr, cg, cb, 255)

        # Rounded rectangle
        rect = (bubble_left, _BUBBLE_TOP, bubble_right, _BUBBLE_BOTTOM)
        try:
            draw.rounded_rectangle(rect, radius=_BUBBLE_RADIUS,
                                   fill=fill, outline=border, width=2)
        except AttributeError:
            draw.rectangle(rect, fill=fill, outline=border, width=2)

        # Triangle pointer from bottom center of bubble
        mid_x = CANVAS_W // 2
        tri = [
            (mid_x - _TRI_WIDTH // 2, _BUBBLE_BOTTOM - 1),
            (mid_x + _TRI_WIDTH // 2, _BUBBLE_BOTTOM - 1),
            (mid_x, _TRI_TIP_Y),
        ]
        draw.polygon(tri, fill=fill)
        draw.line([tri[0], tri[2]], fill=border, width=2)
        draw.line([tri[1], tri[2]], fill=border, width=2)

        # Centered text
        tx = (bubble_left + bubble_right) // 2 - tw // 2
        ty = (_BUBBLE_TOP + _BUBBLE_BOTTOM) // 2 - th // 2
        draw.text((tx, ty), text, fill=(40, 40, 40, 255), font=self._font)

    def _draw_info_bar(self, canvas: Image.Image, model_name: str, ratio: float):
        """Draw model name label and context health bar below the sprite."""
        draw = ImageDraw.Draw(canvas)
        ratio = max(0.0, min(1.0, ratio))

        # -- Model name label (centered, white, bold) --
        label = model_name or FALLBACK_MODEL_NAME
        if len(label) > 30:
            label = label[:27] + "..."

        try:
            bbox = draw.textbbox((0, 0), label, font=self._small_font)
            tw = bbox[2] - bbox[0]
        except AttributeError:
            tw, _ = draw.textsize(label, font=self._small_font)

        tx = (CANVAS_W - tw) // 2
        draw.text((tx, _INFO_Y), label, fill=(255, 255, 255, 240), font=self._small_font)

        # -- "Context" label (centered above bar) --
        ctx_label = "Context"
        try:
            bbox = draw.textbbox((0, 0), ctx_label, font=self._label_font)
            ltw = bbox[2] - bbox[0]
        except AttributeError:
            ltw, _ = draw.textsize(ctx_label, font=self._label_font)

        ltx = (CANVAS_W - ltw) // 2
        draw.text((ltx, _BAR_LABEL_Y), ctx_label, fill=(160, 160, 180, 200), font=self._label_font)

        # -- Health bar --
        bar_left = (CANVAS_W - _BAR_WIDTH) // 2
        bar_right = bar_left + _BAR_WIDTH

        # Background (dark, semi-transparent)
        bg_rect = (bar_left, _BAR_Y, bar_right, _BAR_Y + _BAR_HEIGHT)
        try:
            draw.rounded_rectangle(bg_rect, radius=_BAR_RADIUS,
                                   fill=(30, 30, 40, 160), outline=(60, 60, 80, 200), width=1)
        except AttributeError:
            draw.rectangle(bg_rect, fill=(30, 30, 40, 160), outline=(60, 60, 80, 200), width=1)

        # Filled portion
        if ratio > 0.005:
            fill_width = max(2 * _BAR_RADIUS, int(_BAR_WIDTH * ratio))
            fill_right = bar_left + fill_width

            # Color: green → yellow → red
            if ratio < 0.5:
                r = int(80 + 175 * (ratio / 0.5))
                g = 200
            else:
                r = 255
                g = int(200 * (1.0 - (ratio - 0.5) / 0.5))
            b = 60
            fill_color = (r, g, b, 230)

            fill_rect = (bar_left, _BAR_Y, fill_right, _BAR_Y + _BAR_HEIGHT)
            try:
                draw.rounded_rectangle(fill_rect, radius=_BAR_RADIUS,
                                       fill=fill_color)
            except AttributeError:
                draw.rectangle(fill_rect, fill=fill_color)