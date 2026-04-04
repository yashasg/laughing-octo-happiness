#include "sprite_renderer.h"
#include "anim_state.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
SpriteRenderer::SpriteRenderer() = default;

SpriteRenderer::~SpriteRenderer() {
    if (m_idle_sheet.id > 0) UnloadTexture(m_idle_sheet);
    if (m_run_sheet.id  > 0) UnloadTexture(m_run_sheet);
}

void SpriteRenderer::load(const std::string& idle_path, const std::string& run_path) {
    m_idle_sheet = LoadTexture(idle_path.c_str());
    m_run_sheet  = LoadTexture(run_path.c_str());

    // Pixel-art spritesheets look best with nearest-neighbor scaling.
    SetTextureFilter(m_idle_sheet, TEXTURE_FILTER_POINT);
    SetTextureFilter(m_run_sheet,  TEXTURE_FILTER_POINT);

    m_anim.idle_frame_count = (m_idle_sheet.width > 0) ? m_idle_sheet.width / SPRITE_WIDTH : IDLE_FRAMES;
    m_anim.run_frame_count  = (m_run_sheet.width  > 0) ? m_run_sheet.width  / SPRITE_WIDTH : RUN_FRAMES;
}

// ---------------------------------------------------------------------------
// Animation tick (call once per game-loop iteration)
// ---------------------------------------------------------------------------
void SpriteRenderer::tick(CopilotStatus status) {
    m_anim.tick(status);
}

// ---------------------------------------------------------------------------
// Public draw entry point
// ---------------------------------------------------------------------------
void SpriteRenderer::draw(CopilotStatus status) const {
    draw_sprite(status);
}

// ---------------------------------------------------------------------------
// Sprite frame
// ---------------------------------------------------------------------------
void SpriteRenderer::draw_sprite(CopilotStatus status) const {
    bool is_busy = (status == CopilotStatus::BUSY);
    const Texture2D& sheet = is_busy ? m_run_sheet : m_idle_sheet;
    int frame_count        = is_busy ? m_anim.run_frame_count : m_anim.idle_frame_count;

    if (sheet.id == 0 || frame_count <= 0) return;

    int fi = m_anim.frame_index % frame_count;
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
