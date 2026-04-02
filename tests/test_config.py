"""Tests for config module."""

import sys
from pathlib import Path

from config import (
    CopilotStatus,
    BUBBLE_COLORS,
    DEFAULT_LABELS,
    COPILOT_STATE_DIR,
    SPRITES_DIR,
    SPRITE_WIDTH,
    SPRITE_HEIGHT,
    SPRITE_DISPLAY_SCALE,
    SPRITE_DISPLAY_W,
    SPRITE_DISPLAY_H,
    CANVAS_W,
    CANVAS_H,
    FRAME_RATE,
    FRAME_DELAY,
)


class TestCopilotStatus:
    def test_enum_values(self):
        assert CopilotStatus.IDLE.value == "idle"
        assert CopilotStatus.WAITING.value == "waiting"
        assert CopilotStatus.BUSY.value == "busy"

    def test_all_statuses_have_bubble_color(self):
        for status in CopilotStatus:
            assert status in BUBBLE_COLORS, f"Missing BUBBLE_COLORS for {status}"
            color = BUBBLE_COLORS[status]
            assert len(color) == 3
            assert all(0 <= c <= 255 for c in color)

    def test_all_statuses_have_default_label(self):
        for status in CopilotStatus:
            assert status in DEFAULT_LABELS, f"Missing DEFAULT_LABELS for {status}"
            assert isinstance(DEFAULT_LABELS[status], str)
            assert len(DEFAULT_LABELS[status]) > 0


class TestPaths:
    def test_copilot_state_dir_uses_home(self):
        """COPILOT_STATE_DIR must be under the user's home directory."""
        home = Path.home()
        assert str(COPILOT_STATE_DIR).startswith(str(home)), (
            f"COPILOT_STATE_DIR ({COPILOT_STATE_DIR}) should be under {home}"
        )

    def test_copilot_state_dir_ends_with_session_state(self):
        assert COPILOT_STATE_DIR.name == "session-state"
        assert COPILOT_STATE_DIR.parent.name == ".copilot"

    def test_sprites_dir_is_relative_to_project(self):
        assert SPRITES_DIR.name == "sprites"


class TestDerivedConstants:
    def test_display_dimensions_match_scale(self):
        assert SPRITE_DISPLAY_W == SPRITE_WIDTH * SPRITE_DISPLAY_SCALE
        assert SPRITE_DISPLAY_H == SPRITE_HEIGHT * SPRITE_DISPLAY_SCALE

    def test_canvas_fits_sprite(self):
        assert CANVAS_W >= SPRITE_DISPLAY_W
        assert CANVAS_H >= SPRITE_DISPLAY_H

    def test_frame_delay_matches_rate(self):
        assert FRAME_DELAY == 1000 // FRAME_RATE

    def test_frame_rate_positive(self):
        assert FRAME_RATE > 0
        assert FRAME_DELAY > 0
