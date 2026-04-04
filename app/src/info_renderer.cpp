#include "info_renderer.h"
#include "render_logic.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdio>

// ---------------------------------------------------------------------------
// Platform font discovery
// ---------------------------------------------------------------------------
static std::string find_system_font() {
#if defined(_WIN32)
    const std::vector<std::string> candidates = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/verdana.ttf",
    };
#elif defined(__APPLE__)
    const std::vector<std::string> candidates = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Geneva.ttf",
    };
#else
    const std::vector<std::string> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    };
#endif
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) return path;
    }
    return "";
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
InfoRenderer::InfoRenderer() = default;

InfoRenderer::~InfoRenderer() {
    if (m_font_loaded) {
        UnloadFont(m_label_font);
    }
}

void InfoRenderer::load(const std::string& font_path) {
    std::string resolved = font_path.empty() ? find_system_font()
                         : std::filesystem::exists(font_path) ? font_path
                         : find_system_font();
    if (!resolved.empty()) {
        m_label_font = LoadFontEx(resolved.c_str(), 8, nullptr, 0);
        SetTextureFilter(m_label_font.texture, TEXTURE_FILTER_POINT);
        m_font_loaded = true;
    }
    // m_font_loaded stays false → draw falls back to GetFontDefault()
}

// ---------------------------------------------------------------------------
// Format a token count as compact string: 1234 → "1.2k", 135427 → "135k"
// ---------------------------------------------------------------------------
static std::string format_tokens(size_t tokens) {
    if (tokens < 1000) return std::to_string(tokens);
    char buf[32];
    if (tokens < 10000)
        std::snprintf(buf, sizeof(buf), "%.1fk", static_cast<double>(tokens) / 1000.0);
    else
        std::snprintf(buf, sizeof(buf), "%zuk", tokens / 1000);
    return buf;
}

// ---------------------------------------------------------------------------
// Context health bar
// ---------------------------------------------------------------------------
void InfoRenderer::draw(float ratio, size_t current_tokens, size_t token_limit) const {
    ratio = std::max(0.0f, std::min(1.0f, ratio));

    Font  lbl      = m_font_loaded ? m_label_font : GetFontDefault();
    float lbl_size = static_cast<float>(lbl.baseSize);

    // Context label — show token counts when available, else "Context"
    std::string label_str;
    if (token_limit > 0 && current_tokens > 0) {
        label_str = format_tokens(current_tokens) + " / " + format_tokens(token_limit);
    } else {
        label_str = "Context";
    }
    const char* ctx = label_str.c_str();
    Vector2 ctx_sz = MeasureTextEx(lbl, ctx, lbl_size, 1.0f);
    float ltx = (CANVAS_W - ctx_sz.x) / 2.0f;
    DrawTextEx(lbl, ctx, { ltx, static_cast<float>(BAR_LABEL_Y) },
               lbl_size, 1.0f, { 160, 160, 180, 200 });

    // Health bar background
    float bar_left = (CANVAS_W - BAR_WIDTH) / 2.0f;
    float bar_rnd  = std::min(1.0f, (2.0f * BAR_RADIUS) / static_cast<float>(BAR_HEIGHT));
    Rectangle bg   = { bar_left, static_cast<float>(BAR_Y),
                        static_cast<float>(BAR_WIDTH), static_cast<float>(BAR_HEIGHT) };
    DrawRectangleRounded(bg, bar_rnd, 8, { 30, 30, 40, 160 });
    DrawRectangleRoundedLinesEx(bg, bar_rnd, 8, 1.0f, { 60, 60, 80, 200 });

    // Health bar fill — green (low) → yellow (mid) → red (high)
    if (ratio > 0.005f) {
        float fill_w = compute_bar_fill_width(ratio, static_cast<float>(BAR_WIDTH));
        float fill_rnd = std::min(1.0f, (2.0f * BAR_RADIUS) / std::min(fill_w, static_cast<float>(BAR_HEIGHT)));
        Rectangle fill_rect = { bar_left, static_cast<float>(BAR_Y), fill_w, static_cast<float>(BAR_HEIGHT) };
        Color fill_color = compute_bar_color(ratio);
        DrawRectangleRounded(fill_rect, fill_rnd, 8, fill_color);
    }
}
