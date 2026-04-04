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
// Per-frame update
// ---------------------------------------------------------------------------
bool InputHandler::process() {
    // Quit on Q or ESC
    if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
        return true;
    }

    // Drag-to-move
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        m_dragging   = true;
        m_drag_start = GetMousePosition();
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        m_dragging = false;
    }
    if (m_dragging) {
        Vector2 mouse = GetMousePosition();
        Vector2 delta = {mouse.x - m_drag_start.x, mouse.y - m_drag_start.y};
        Vector2 pos   = GetWindowPosition();
        SetWindowPosition(static_cast<int>(pos.x + delta.x),
                          static_cast<int>(pos.y + delta.y));
    }

    // Re-assert topmost every ~90 frames (~3 s at 30 fps) to stay above other windows.
    if (++m_topmost_counter >= 90) {
        m_topmost_counter = 0;
        SetWindowState(FLAG_WINDOW_TOPMOST);
    }

    return false;
}
