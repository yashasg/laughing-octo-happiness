#include "config.h"
#include "status_monitor.h"
#include "sprite_renderer.h"

#include "raylib.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>

int main() {
    std::cout << "Copilot Buddy (C++) v0.1.0\n";

    // -----------------------------------------------------------------------
    // Window setup
    // -----------------------------------------------------------------------
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_TOPMOST
                 | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(CANVAS_W, CANVAS_H, "Copilot Buddy");
    SetTargetFPS(TARGET_FPS);
    SetExitKey(KEY_NULL); // disable ESC quit so we control shutdown ourselves

    // Position bottom-right of primary monitor
    int monIdx = GetCurrentMonitor();
    int monW   = GetMonitorWidth(monIdx);
    int monH   = GetMonitorHeight(monIdx);
    SetWindowPosition(monW + OVERLAY_DEFAULT_X - CANVAS_W,
                      monH + OVERLAY_DEFAULT_Y - CANVAS_H);

    // -----------------------------------------------------------------------
    // Load resources
    // -----------------------------------------------------------------------
    SpriteRenderer renderer;
    std::string base = GetApplicationDirectory();
    renderer.load(base + "resources/IDLE.png", base + "resources/RUN.png");

    // -----------------------------------------------------------------------
    // Status monitor (background threads, thread-safe getters)
    // -----------------------------------------------------------------------
    StatusMonitor monitor([](CopilotStatus s, const std::string& text) {
        // Logging only — main loop polls via atomic getters each frame
        const char* label = "IDLE";
        if (s == CopilotStatus::WAITING) label = "WAITING";
        else if (s == CopilotStatus::BUSY) label = "BUSY";
        std::cout << "[status] " << label << " — " << text << "\n";
    });
    monitor.start();

    // -----------------------------------------------------------------------
    // Drag-to-move state
    // -----------------------------------------------------------------------
    bool    dragging   = false;
    Vector2 drag_start = {0.f, 0.f};

    // -----------------------------------------------------------------------
    // Periodic topmost re-assertion counter
    // -----------------------------------------------------------------------
    int topmost_counter = 0;

    // -----------------------------------------------------------------------
    // Game loop
    // -----------------------------------------------------------------------
    while (!WindowShouldClose()) {

        // 1. Read status (all thread-safe)
        CopilotStatus status      = monitor.status();
        std::string   status_text = monitor.status_text();
        std::string   model_name  = monitor.model_name();
        size_t        ctx_bytes   = monitor.context_bytes();
        float         ctx_ratio   = std::min(1.0f,
                                        static_cast<float>(ctx_bytes)
                                        / static_cast<float>(CONTEXT_MAX_BYTES));

        // 2. Right-click → quit
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            break;
        }

        // 3. Drag-to-move
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            dragging   = true;
            drag_start = GetMousePosition();
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            dragging = false;
        }
        if (dragging) {
            Vector2 mouse = GetMousePosition();
            Vector2 delta = {mouse.x - drag_start.x, mouse.y - drag_start.y};
            Vector2 pos   = GetWindowPosition();
            SetWindowPosition(static_cast<int>(pos.x + delta.x),
                              static_cast<int>(pos.y + delta.y));
        }

        // 4. Advance animation
        renderer.tick(status);

        // 5. Re-assert topmost every ~90 frames (~3 s at 30 fps)
        if (++topmost_counter >= 90) {
            topmost_counter = 0;
            SetWindowState(FLAG_WINDOW_TOPMOST);
        }

        // 6. Draw
        BeginDrawing();
        ClearBackground(BLANK);
        renderer.draw(status, status_text, model_name, ctx_ratio);
        EndDrawing();
    }

    // -----------------------------------------------------------------------
    // Clean shutdown
    // -----------------------------------------------------------------------
    monitor.stop();
    CloseWindow();
    return 0;
}
