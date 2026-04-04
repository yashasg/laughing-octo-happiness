#pragma once

#include "raylib.h"

/// Handles all per-frame user input and window-management concerns:
/// - Q / ESC quit detection
/// - Left-click drag-to-move
/// - Raw GLFW right-click callback chain (macOS borderless window workaround)
/// - Periodic FLAG_WINDOW_TOPMOST re-assertion
class InputHandler {
public:
    InputHandler() = default;

    /// Install the raw GLFW mouse-button callback chain.
    /// Must be called after InitWindow().
    void init();

    /// Process one frame of input. Returns true if the app should quit.
    /// Handles drag-to-move and topmost re-assertion internally.
    bool process();

private:
    bool    m_dragging        = false;
    Vector2 m_drag_start      = {0.f, 0.f};
    int     m_topmost_counter = 0;
};
