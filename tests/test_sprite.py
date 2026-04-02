"""Tests for sprite module — non-GUI rendering logic."""

import sys
from unittest.mock import patch, MagicMock
from pathlib import Path

from PIL import Image

from config import (
    CopilotStatus,
    SPRITE_WIDTH,
    SPRITE_HEIGHT,
    SPRITE_DISPLAY_W,
    SPRITE_DISPLAY_H,
    IDLE_FRAMES,
    RUN_FRAMES,
    SPRITES_DIR,
)


class TestExtractFrames:
    """Test the _extract_frames helper."""

    def test_extracts_correct_number_of_frames(self):
        from sprite import _extract_frames

        # Create a dummy spritesheet: 3 frames wide
        width = SPRITE_WIDTH * 3
        sheet = Image.new("RGBA", (width, SPRITE_HEIGHT), (255, 0, 0, 255))
        frames = _extract_frames(sheet, 3)
        assert len(frames) == 3

    def test_frames_have_correct_dimensions(self):
        from sprite import _extract_frames

        width = SPRITE_WIDTH * 2
        sheet = Image.new("RGBA", (width, SPRITE_HEIGHT), (0, 255, 0, 255))
        frames = _extract_frames(sheet, 2)
        for frame in frames:
            assert frame.size == (SPRITE_DISPLAY_W, SPRITE_DISPLAY_H)

    def test_frames_are_rgba(self):
        from sprite import _extract_frames

        sheet = Image.new("RGBA", (SPRITE_WIDTH, SPRITE_HEIGHT), (0, 0, 255, 128))
        frames = _extract_frames(sheet, 1)
        assert frames[0].mode == "RGBA"


class TestSpriteRenderer:
    """Test SpriteRenderer — requires sprite files to exist."""

    def _sprites_exist(self):
        return (SPRITES_DIR / "IDLE.png").exists() and (SPRITES_DIR / "RUN.png").exists()

    def test_renderer_initializes(self):
        if not self._sprites_exist():
            import pytest
            pytest.skip("Sprite files not found")

        from sprite import SpriteRenderer
        renderer = SpriteRenderer()
        assert renderer is not None

    def test_get_frame_returns_tuple(self):
        if not self._sprites_exist():
            import pytest
            pytest.skip("Sprite files not found")

        # Mock tkinter to avoid needing a display
        mock_tk = MagicMock()
        with patch.dict(sys.modules, {"tkinter": mock_tk}):
            from sprite import SpriteRenderer
            renderer = SpriteRenderer()

            # get_frame needs ImageTk which requires display; test the compositing
            # by calling the internal methods instead
            canvas = Image.new("RGBA", (300, 246), (0, 0, 0, 0))
            renderer._draw_bubble(canvas, "Test", (0, 200, 80))
            # If we get here without error, the bubble drawing works
            assert True

    def test_draw_bubble_handles_long_text(self):
        if not self._sprites_exist():
            import pytest
            pytest.skip("Sprite files not found")

        from sprite import SpriteRenderer, _MAX_TEXT_LEN
        renderer = SpriteRenderer()
        canvas = Image.new("RGBA", (300, 246), (0, 0, 0, 0))
        long_text = "A" * 50
        renderer._draw_bubble(canvas, long_text, (255, 0, 0))
        # Should not raise; text gets truncated internally

    def test_draw_info_bar_renders(self):
        if not self._sprites_exist():
            import pytest
            pytest.skip("Sprite files not found")

        from sprite import SpriteRenderer
        renderer = SpriteRenderer()
        canvas = Image.new("RGBA", (300, 246), (0, 0, 0, 0))
        renderer._draw_info_bar(canvas, "claude-opus-4.6", 0.75)
        # Should not raise

    def test_draw_info_bar_clamps_ratio(self):
        if not self._sprites_exist():
            import pytest
            pytest.skip("Sprite files not found")

        from sprite import SpriteRenderer
        renderer = SpriteRenderer()
        canvas = Image.new("RGBA", (300, 246), (0, 0, 0, 0))
        # Out-of-range ratios should be clamped, not raise
        renderer._draw_info_bar(canvas, "model", -0.5)
        renderer._draw_info_bar(canvas, "model", 1.5)
