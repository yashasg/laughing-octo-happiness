#include "sprite_renderer.h"

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
SpriteRenderer::SpriteRenderer() = default;

SpriteRenderer::~SpriteRenderer() {
    if (m_idle_sheet.id > 0) UnloadTexture(m_idle_sheet);
    if (m_run_sheet.id  > 0) UnloadTexture(m_run_sheet);
    if (m_fonts_loaded) {
        UnloadFont(m_font);
        UnloadFont(m_small_font);
        UnloadFont(m_label_font);
    }
}

void SpriteRenderer::load(const std::string& idle_path, const std::string& run_path,
                           const std::string& font_path) {
    m_idle_sheet = LoadTexture(idle_path.c_str());
    m_run_sheet  = LoadTexture(run_path.c_str());

    // Pixel-art spritesheets look best with nearest-neighbor scaling.
    SetTextureFilter(m_idle_sheet, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_run_sheet,  TEXTURE_FILTER_POINT);

    m_idle_frame_count = (m_idle_sheet.width > 0) ? m_idle_sheet.width / SPRITE_WIDTH : IDLE_FRAMES;
    m_run_frame_count  = (m_run_sheet.width  > 0) ? m_run_sheet.width  / SPRITE_WIDTH : RUN_FRAMES;

    std::string resolved = font_path.empty() ? find_system_font() : font_path;

    if (!resolved.empty()) {
        m_font       = LoadFontEx(resolved.c_str(), 15, nullptr, 0);
        m_small_font = LoadFontEx(resolved.c_str(), 14, nullptr, 0);
        m_label_font = LoadFontEx(resolved.c_str(), 10, nullptr, 0);
        SetTextureFilter(m_font.texture,       TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(m_small_font.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(m_label_font.texture, TEXTURE_FILTER_BILINEAR);
        m_fonts_loaded = true;
    }
    // m_fonts_loaded stays false → draw helpers fall back to GetFontDefault()
}

// ---------------------------------------------------------------------------
// Animation tick (call once per game-loop iteration)
// ---------------------------------------------------------------------------
void SpriteRenderer::tick(CopilotStatus status) {
    // Scale up by FRAME_RATE each tick; wrap when we hit TARGET_FPS.
    // This gives frame advances at exactly FRAME_RATE / TARGET_FPS = 18/30 per tick.
    m_anim_counter += FRAME_RATE;
    if (m_anim_counter >= TARGET_FPS) {
        m_anim_counter -= TARGET_FPS;
        const int count = (status == CopilotStatus::BUSY) ? m_run_frame_count : m_idle_frame_count;
        if (count > 0) m_frame_index = (m_frame_index + 1) % count;
    }
}

// ---------------------------------------------------------------------------
// Public draw entry point
// ---------------------------------------------------------------------------
void SpriteRenderer::draw(CopilotStatus status, const std::string& status_text,
                           const std::string& model_name, float context_ratio) const {
    const std::string& text = status_text.empty()
        ? std::string(status_label(status))
        : status_text;
    draw_bubble(status, text);
    draw_sprite(status);
    draw_info_bar(model_name, context_ratio);
}

// ---------------------------------------------------------------------------
// Speech bubble
// ---------------------------------------------------------------------------
void SpriteRenderer::draw_bubble(CopilotStatus status, const std::string& text) const {
    // Truncate long text
    std::string display = text;
    if (static_cast<int>(display.size()) > MAX_TEXT_LEN)
        display = display.substr(0, MAX_TEXT_LEN - 3) + "...";

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
// Sprite frame
// ---------------------------------------------------------------------------
void SpriteRenderer::draw_sprite(CopilotStatus status) const {
    bool is_busy = (status == CopilotStatus::BUSY);
    const Texture2D& sheet = is_busy ? m_run_sheet : m_idle_sheet;
    int frame_count        = is_busy ? m_run_frame_count : m_idle_frame_count;

    if (sheet.id == 0 || frame_count <= 0) return;

    int fi = m_frame_index % frame_count;
    Rectangle src = {
        static_cast<float>(fi * SPRITE_WIDTH), 0.0f,
        static_cast<float>(SPRITE_WIDTH), static_cast<float>(SPRITE_HEIGHT)
    };
    float sprite_x = (CANVAS_W - SPRITE_DISPLAY_W) / 2.0f;
    Rectangle dst = {
        sprite_x, static_cast<float>(SPRITE_Y),
        static_cast<float>(SPRITE_DISPLAY_W), static_cast<float>(SPRITE_DISPLAY_H)
    };
    DrawTexturePro(sheet, src, dst, { 0.0f, 0.0f }, 0.0f, WHITE);
}

// ---------------------------------------------------------------------------
// Info bar: model name + context health bar
// ---------------------------------------------------------------------------
void SpriteRenderer::draw_info_bar(const std::string& model_name, float ratio) const {
    ratio = std::max(0.0f, std::min(1.0f, ratio));

    Font  small      = m_fonts_loaded ? m_small_font : GetFontDefault();
    Font  lbl        = m_fonts_loaded ? m_label_font : GetFontDefault();
    float small_size = static_cast<float>(small.baseSize);
    float lbl_size   = static_cast<float>(lbl.baseSize);

    // Model name
    std::string name = model_name.empty() ? FALLBACK_MODEL_NAME : model_name;
    if (static_cast<int>(name.size()) > 30) name = name.substr(0, 27) + "...";
    Vector2 name_sz = MeasureTextEx(small, name.c_str(), small_size, 1.0f);
    float tx = (CANVAS_W - name_sz.x) / 2.0f;
    DrawTextEx(small, name.c_str(), { tx, static_cast<float>(INFO_Y) },
               small_size, 1.0f, { 255, 255, 255, 240 });

    // "Context" label
    const char* ctx = "Context";
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
        float fill_w = std::max(2.0f * BAR_RADIUS, BAR_WIDTH * ratio);
        float fill_rnd = std::min(1.0f, (2.0f * BAR_RADIUS) / std::min(fill_w, static_cast<float>(BAR_HEIGHT)));
        Rectangle fill_rect = { bar_left, static_cast<float>(BAR_Y), fill_w, static_cast<float>(BAR_HEIGHT) };

        unsigned char r, g;
        const unsigned char b = 60;
        if (ratio < 0.5f) {
            r = static_cast<unsigned char>(80.0f + 175.0f * (ratio / 0.5f));
            g = 200;
        } else {
            r = 255;
            g = static_cast<unsigned char>(200.0f * (1.0f - (ratio - 0.5f) / 0.5f));
        }
        DrawRectangleRounded(fill_rect, fill_rnd, 8, { r, g, b, 230 });
    }
}
