#include "config.h"
#include "auth.h"
#include "status_monitor.h"
#include "sprite_renderer.h"
#include "text_renderer.h"
#include "info_renderer.h"
#include "input_handler.h"

#include "raylib.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
#ifndef COPILOT_BUDDY_VERSION
#define COPILOT_BUDDY_VERSION "dev"
#endif
    std::cout << "Copilot Buddy (C++) v" COPILOT_BUDDY_VERSION "\n";

    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--verbose" || std::string(argv[i]) == "-v")
            verbose = true;
    }

    ensure_github_token();

    // --- Window setup --------------------------------------------------------
    SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_TOPMOST
                 | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(CANVAS_W, CANVAS_H, "Copilot Buddy");
    SetTargetFPS(TARGET_FPS);
    SetExitKey(KEY_ESCAPE);

    InputHandler input;
    bool should_quit = false;
    input.on_key_pressed([&should_quit](int key) {
        if (key == KEY_ESCAPE || key == KEY_Q) should_quit = true;
    });
    input.init();

    // Position bottom-right of primary monitor
    int mon   = GetCurrentMonitor();
    int monW  = GetMonitorWidth(mon);
    int monH  = GetMonitorHeight(mon);
    SetWindowPosition(monW + OVERLAY_DEFAULT_X - CANVAS_W,
                      monH + OVERLAY_DEFAULT_Y - CANVAS_H);

    // --- Resources -----------------------------------------------------------
    SpriteRenderer renderer;
    TextRenderer   text_renderer;
    InfoRenderer   info_renderer;
    std::string base = GetApplicationDirectory();
    renderer.load(base + "resources/IDLE.png", base + "resources/RUN.png");
    std::string font_path = base + "resources/fonts/PixelifySans-Regular.ttf";
    text_renderer.load(font_path);
    info_renderer.load(font_path);

    // --- Status monitor (background threads) ---------------------------------
    StatusMonitor monitor([verbose](CopilotStatus s, const std::string& text) {
        if (!verbose) return;
        std::cout << "[status] " << status_label(s) << " — " << text << "\n";
    });
    monitor.start();

    // --- Game loop -----------------------------------------------------------
    while (!WindowShouldClose() && !should_quit) {
        CopilotStatus status     = monitor.status();
        std::string   model_name = monitor.model_name();
        size_t        tok_used   = monitor.current_tokens();
        size_t        tok_limit  = model_context_limit(model_name);
        float         ctx_ratio  = compute_context_ratio(
                                       tok_used, tok_limit, monitor.context_bytes());

        std::string bubble_text = resolve_bubble_text(
            status, monitor.status_text(), monitor.idle_text());

        input.process();
        renderer.tick(status);

        BeginDrawing();
        ClearBackground(BLANK);
        text_renderer.draw_bubble(status, bubble_text);
        renderer.draw(status);
        text_renderer.draw_model_name(model_name);
        info_renderer.draw(ctx_ratio, tok_used, tok_limit);
        EndDrawing();
    }

    // --- Shutdown -------------------------------------------------------------
    monitor.stop();
    CloseWindow();
    return 0;
}
