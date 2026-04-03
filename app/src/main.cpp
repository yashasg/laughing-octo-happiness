#include "config.h"
#include "status_monitor.h"
#include "sprite_renderer.h"
#include "text_renderer.h"
#include "info_renderer.h"
#include "input_handler.h"

#include "raylib.h"

#include <iostream>
#include <string>
#include <algorithm>

int main() {
#ifndef COPILOT_BUDDY_VERSION
#define COPILOT_BUDDY_VERSION "dev"
#endif
    std::cout << "Copilot Buddy (C++) v" COPILOT_BUDDY_VERSION "\n";

    // -----------------------------------------------------------------------
    // Window setup
    // -----------------------------------------------------------------------
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_TOPMOST
                 | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(CANVAS_W, CANVAS_H, "Copilot Buddy");
    SetTargetFPS(TARGET_FPS);
    SetExitKey(KEY_ESCAPE); // ESC quits (also caught manually in loop for clean shutdown)

    // Install raw GLFW right-click callback and set up input handling
    InputHandler input;
    input.init();

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
    TextRenderer   text_renderer;
    InfoRenderer   info_renderer;
    std::string base = GetApplicationDirectory();
    renderer.load(base + "resources/IDLE.png", base + "resources/RUN.png");
    text_renderer.load();
    info_renderer.load();

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

        // 2. Input (quit, drag-to-move, topmost re-assertion)
        if (input.process()) break;

        // 3. Advance animation
        renderer.tick(status);

        // 4. Draw
        BeginDrawing();
        ClearBackground(BLANK);
        const std::string& bubble_text = status_text.empty()
            ? std::string(status_label(status))
            : status_text;
        text_renderer.draw_bubble(status, bubble_text);
        renderer.draw(status);
        text_renderer.draw_model_name(model_name);
        info_renderer.draw(ctx_ratio);
        EndDrawing();
    }

    // -----------------------------------------------------------------------
    // Clean shutdown
    // -----------------------------------------------------------------------
    monitor.stop();
    CloseWindow();
    return 0;
}
