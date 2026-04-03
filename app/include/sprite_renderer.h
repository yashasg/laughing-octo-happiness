#pragma once

#include "config.h"
#include "raylib.h"
#include <string>

/// GPU-accelerated sprite renderer using raylib.
/// Loads spritesheets once; all draw calls use GPU textures.
class SpriteRenderer {
public:
    SpriteRenderer();
    ~SpriteRenderer();

    /// Load sprite sheets and fonts. Call after InitWindow().
    void load(const std::string& idle_path, const std::string& run_path,
              const std::string& font_path = "");

    /// Advance animation frame counter (call once per game-loop tick).
    void tick(CopilotStatus status);

    /// Draw the full overlay frame (speech bubble + sprite + info bar).
    /// Call between BeginDrawing()/EndDrawing().
    void draw(CopilotStatus status,
              const std::string& status_text,
              const std::string& model_name,
              float context_ratio) const;

private:
    void draw_bubble(CopilotStatus status, const std::string& text) const;
    void draw_sprite(CopilotStatus status) const;
    void draw_info_bar(const std::string& model_name, float ratio) const;

    Texture2D m_idle_sheet{};
    Texture2D m_run_sheet{};
    int       m_idle_frame_count = 0;
    int       m_run_frame_count  = 0;
    int       m_frame_index      = 0;
    int       m_anim_counter     = 0;  // sub-frame counter for animation speed

    Font m_font{};         // speech bubble text (15pt)
    Font m_small_font{};   // model name (14pt bold)
    Font m_label_font{};   // "Context" label (10pt)
    bool m_fonts_loaded = false;
};
