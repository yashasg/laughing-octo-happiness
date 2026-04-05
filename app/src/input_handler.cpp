#include "input_handler.h"

#include "GLFW/glfw3.h"

// ---------------------------------------------------------------------------
// Raw GLFW mouse-button callback — chained after raylib's own callback.
// Needed because on macOS borderless floating windows, right-click events
// can be consumed before raylib's input poller sees them.
// ---------------------------------------------------------------------------
static GLFWmousebuttonfun g_prev_mouse_cb = nullptr;

static void mouse_button_cb(GLFWwindow* w, int button, int action, int mods) {
    // Forward to raylib first so left-click / drag still works.
    if (g_prev_mouse_cb) g_prev_mouse_cb(w, button, action, mods);
    // Right-click is reserved for a future context menu.
    (void)button; (void)action; (void)mods;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void InputHandler::init() {
    GLFWwindow* glfwWin = glfwGetCurrentContext();
    g_prev_mouse_cb = glfwSetMouseButtonCallback(glfwWin, mouse_button_cb);
}

// ---------------------------------------------------------------------------
// Event system
// ---------------------------------------------------------------------------
void InputHandler::on_key_pressed(KeyCallback callback) {
    m_key_callbacks.push_back(std::move(callback));
}

void InputHandler::fire_key_pressed(int key) {
    for (auto& cb : m_key_callbacks) cb(key);
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------
void InputHandler::process() {
    // Detect key presses and fire events
    for (int key : {KEY_Q, KEY_ESCAPE}) {
        if (IsKeyPressed(key)) fire_key_pressed(key);
    }

    // Drag-to-move — anchor the click point and keep it under the cursor
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        m_dragging    = true;
        m_drag_anchor = GetMousePosition();
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        m_dragging = false;
    }
    if (m_dragging) {
        Vector2 mouse = GetMousePosition();
        Vector2 win   = GetWindowPosition();
        SetWindowPosition(static_cast<int>(win.x + mouse.x - m_drag_anchor.x),
                          static_cast<int>(win.y + mouse.y - m_drag_anchor.y));
    }

    // Re-assert topmost every ~90 frames (~3 s at 30 fps) to stay above other windows.
    if (++m_topmost_counter >= 90) {
        m_topmost_counter = 0;
        SetWindowState(FLAG_WINDOW_TOPMOST);
    }
}

// ---------------------------------------------------------------------------
// Test helper — friend function to fire key events without raylib.
// ---------------------------------------------------------------------------
void simulate_key_press(InputHandler& handler, int key) {
    handler.fire_key_pressed(key);
}
