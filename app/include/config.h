#pragma once

#include "raylib.h"
#include <string>

// ---------------------------------------------------------------------------
// Status model
// ---------------------------------------------------------------------------
enum class CopilotStatus { IDLE, WAITING, BUSY };

inline const char* status_label(CopilotStatus s) {
    switch (s) {
        case CopilotStatus::IDLE:    return "Zzz...";
        case CopilotStatus::WAITING: return "Waiting on you!";
        case CopilotStatus::BUSY:    return "Working...";
    }
    return "";
}

// ---------------------------------------------------------------------------
// Bubble / accent colors  (RGBA)
// ---------------------------------------------------------------------------
inline Color bubble_color(CopilotStatus s) {
    switch (s) {
        case CopilotStatus::IDLE:    return {70, 130, 220, 255};   // Blue
        case CopilotStatus::WAITING: return {230, 190, 0, 255};    // Yellow
        case CopilotStatus::BUSY:    return {220, 50, 50, 255};   // Red
    }
    return WHITE;
}

// Bubble fill: white with 15 % tint of accent color
inline Color bubble_fill(CopilotStatus s) {
    Color c = bubble_color(s);
    return {
        static_cast<unsigned char>(255 * 0.85f + c.r * 0.15f),
        static_cast<unsigned char>(255 * 0.85f + c.g * 0.15f),
        static_cast<unsigned char>(255 * 0.85f + c.b * 0.15f),
        255
    };
}

// ---------------------------------------------------------------------------
// Spritesheet config
// ---------------------------------------------------------------------------
constexpr int SPRITE_WIDTH        = 96;
constexpr int SPRITE_HEIGHT       = 40;
constexpr int SPRITE_DISPLAY_SCALE = 3;
constexpr int SPRITE_DISPLAY_W    = SPRITE_WIDTH * SPRITE_DISPLAY_SCALE;   // 288
constexpr int SPRITE_DISPLAY_H    = SPRITE_HEIGHT * SPRITE_DISPLAY_SCALE;  // 120
constexpr int IDLE_FRAMES         = 10;
constexpr int RUN_FRAMES          = 16;

// ---------------------------------------------------------------------------
// Canvas / window dimensions
// ---------------------------------------------------------------------------
constexpr int CANVAS_W = 300;
constexpr int CANVAS_H = 246;

// ---------------------------------------------------------------------------
// Layout constants (canvas-relative)
// ---------------------------------------------------------------------------
constexpr int BUBBLE_TOP        = 6;
constexpr int BUBBLE_BOTTOM     = 52;
constexpr int BUBBLE_RADIUS     = 9;
constexpr int BUBBLE_MIN_WIDTH  = 105;
constexpr int BUBBLE_PAD_X      = 18;
constexpr int TRI_WIDTH         = 10;
constexpr int TRI_HEIGHT        = 8;
constexpr int TRI_TIP_Y         = BUBBLE_BOTTOM + TRI_HEIGHT;  // 60
constexpr int SPRITE_Y          = 62;
constexpr int MAX_TEXT_LEN      = 30;

// Info bar layout (below sprite)
constexpr int INFO_Y            = SPRITE_Y + SPRITE_DISPLAY_H + 4;   // model text top
constexpr int BAR_LABEL_Y       = INFO_Y + 20;                       // "Context" label top
constexpr int BAR_Y             = BAR_LABEL_Y + 14;                  // health bar top
constexpr int BAR_HEIGHT        = 10;
constexpr int BAR_WIDTH         = 180;
constexpr int BAR_RADIUS        = 5;

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------
constexpr int FRAME_RATE  = 18;
constexpr int TARGET_FPS  = 30;   // raylib target FPS (higher than animation FPS for smooth input)

// ---------------------------------------------------------------------------
// Overlay positioning  (offsets from screen edges)
// ---------------------------------------------------------------------------
constexpr int OVERLAY_DEFAULT_X = -260;
constexpr int OVERLAY_DEFAULT_Y = -320;

// ---------------------------------------------------------------------------
// Status monitor
// ---------------------------------------------------------------------------
constexpr int    POLL_INTERVAL_MS = 2000;
constexpr size_t TAIL_READ_BYTES  = 8192;

// Model / context info bar
inline const std::string FALLBACK_MODEL_NAME = "Unknown";
constexpr size_t CONTEXT_MAX_BYTES = 2 * 1024 * 1024;  // 2 MB (file-size fallback)
constexpr size_t DEFAULT_CONTEXT_LIMIT = 200000;        // default token limit when model is unknown

// Known model context windows (tokens).
// Used to compute the health bar ratio without an RPC call.
inline size_t model_context_limit(const std::string& model_id) {
    if (model_id.find("claude-opus-4.6-1m") != std::string::npos) return 1000000;
    if (model_id.find("claude-opus-4.5")    != std::string::npos) return 200000;
    if (model_id.find("claude-opus-4.6")    != std::string::npos) return 200000;
    if (model_id.find("claude-sonnet")      != std::string::npos) return 200000;
    if (model_id.find("claude-haiku")       != std::string::npos) return 200000;
    if (model_id.find("gpt-5")             != std::string::npos) return 1000000;
    if (model_id.find("gpt-4.1")           != std::string::npos) return 1000000;
    if (model_id.find("gpt-4o")            != std::string::npos) return 128000;
    return DEFAULT_CONTEXT_LIMIT;
}

#define BACKGROUND_COLOR      CLITERAL(Color){ 0, 0, 0, 50 }           // translucent black