#include "text_renderer.h"
#include "render_logic.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

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
TextRenderer::TextRenderer() = default;

TextRenderer::~TextRenderer() {
    if (m_fonts_loaded) {
        UnloadFont(m_font);
        UnloadFont(m_small_font);
    }
}

void TextRenderer::load(const std::string& font_path) {
    std::string resolved = font_path.empty() ? find_system_font() : font_path;
    if (!resolved.empty()) {
        m_font       = LoadFontEx(resolved.c_str(), 15, nullptr, 0);
        m_small_font = LoadFontEx(resolved.c_str(), 14, nullptr, 0);
        SetTextureFilter(m_font.texture,       TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(m_small_font.texture, TEXTURE_FILTER_BILINEAR);
        m_fonts_loaded = true;
    }
    // m_fonts_loaded stays false → draw helpers fall back to GetFontDefault()
}

// ---------------------------------------------------------------------------
// Speech bubble
// ---------------------------------------------------------------------------
void TextRenderer::draw_bubble(CopilotStatus status, const std::string& text) const {
    // Truncate long text
    std::string display = truncate_text(text, MAX_TEXT_LEN);

    Color accent = bubble_color(status);
    Color fill   = bubble_fill(status);

    Font  font      = m_fonts_loaded ? m_font : GetFontDefault();
    float font_size = static_cast<float>(font.baseSize);

    Vector2 text_sz = MeasureTextEx(font, display.c_str(), font_size, 1.0f);
    float tw = text_sz.x;
    float th = text_sz.y;

    float bubble_w    = std::max(static_cast<float>(BUBBLE_MIN_WIDTH), tw + BUBBLE_PAD_X * 2.0f);
    float bubble_h    = static_cast<float>(BUBBLE_BOTTOM - BUBBLE_TOP);
    float bubble_left = (CANVAS_W - bubble_w) / 2.0f;

    float roundness = std::min(1.0f, (2.0f * BUBBLE_RADIUS) / std::min(bubble_w, bubble_h));

    Rectangle rect = { bubble_left, static_cast<float>(BUBBLE_TOP), bubble_w, bubble_h };
    DrawRectangleRounded(rect, roundness, 12, fill);
    DrawRectangleRoundedLinesEx(rect, roundness, 12, 2.0f, accent);

    // Triangle pointer — fill covers the bubble's bottom border, then side lines are drawn.
    float mid_x     = CANVAS_W / 2.0f;
    float tri_top_y = static_cast<float>(BUBBLE_BOTTOM - 1);
    float tri_tip_y = static_cast<float>(TRI_TIP_Y);
    Vector2 v_left  = { mid_x - TRI_WIDTH / 2.0f, tri_top_y };
    Vector2 v_tip   = { mid_x,                     tri_tip_y };
    Vector2 v_right = { mid_x + TRI_WIDTH / 2.0f,  tri_top_y };

    // Raylib requires CCW winding in screen-space (y-down).
    // v_left → v_tip → v_right traces CCW with y increasing downward.
    DrawTriangle(v_left, v_tip, v_right, fill);
    DrawLineEx(v_left,  v_tip, 2.0f, accent);
    DrawLineEx(v_right, v_tip, 2.0f, accent);

    // Centered text
    float tx = (bubble_left + bubble_left + bubble_w) / 2.0f - tw / 2.0f;
    float ty = (BUBBLE_TOP + BUBBLE_BOTTOM) / 2.0f - th / 2.0f;
    DrawTextEx(font, display.c_str(), { tx, ty }, font_size, 1.0f, { 40, 40, 40, 255 });
}

// ---------------------------------------------------------------------------
// Model name label
// ---------------------------------------------------------------------------
void TextRenderer::draw_model_name(const std::string& model_name) const {
    Font  small      = m_fonts_loaded ? m_small_font : GetFontDefault();
    float small_size = static_cast<float>(small.baseSize);

    std::string name = model_name.empty() ? FALLBACK_MODEL_NAME : model_name;
    if (static_cast<int>(name.size()) > 30) name = name.substr(0, 27) + "...";
    Vector2 name_sz = MeasureTextEx(small, name.c_str(), small_size, 1.0f);
    float tx = (CANVAS_W - name_sz.x) / 2.0f;
    DrawTextEx(small, name.c_str(), { tx, static_cast<float>(INFO_Y) },
               small_size, 1.0f, { 255, 255, 255, 240 });
}
