#pragma once

#include "raylib.h"

#include <functional>
#include <vector>

/// Handles all per-frame user input and window-management concerns:
/// - Key press event dispatch (subscribers handle quit, etc.)
/// - Left-click drag-to-move
/// - Raw GLFW right-click callback chain (macOS borderless window workaround)
/// - Periodic FLAG_WINDOW_TOPMOST re-assertion
class InputHandler {
public:
    using KeyCallback = std::function<void(int key)>;

    InputHandler() = default;

    /// Install the raw GLFW mouse-button callback chain.
    /// Must be called after InitWindow().
    void init();

    /// Process one frame of input.
    /// Fires key events and handles drag-to-move / topmost re-assertion.
    void process();

    /// Register a callback invoked whenever a key press is detected.
    void on_key_pressed(KeyCallback callback);

private:
    void fire_key_pressed(int key);

    std::vector<KeyCallback> m_key_callbacks;
    bool    m_dragging        = false;
    Vector2 m_drag_start      = {0.f, 0.f};
    int     m_topmost_counter = 0;

    /// Allow tests to fire key events without going through raylib.
    friend void simulate_key_press(InputHandler& handler, int key);
};
