#pragma once

#include "config.h"
#include "raylib.h"
#include <string>

/// Renders text elements: the speech bubble (with pointer triangle) and the model name.
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    /// Load fonts for bubble and model name. Call after InitWindow().
    /// If font_path is empty, a system font is discovered automatically.
    void load(const std::string& font_path = "");

    /// Draw the speech bubble with rounded corners, triangle pointer, and centered text.
    void draw_bubble(CopilotStatus status, const std::string& text) const;

    /// Draw the model name label centered below the sprite.
    void draw_model_name(const std::string& model_name) const;

private:
    Font m_font{};        // speech bubble text (16pt)
    Font m_small_font{};  // model name (16pt)
    bool m_fonts_loaded = false;
};
