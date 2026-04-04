#pragma once

#include "raylib.h"
#include <string>

/// Renders the context health bar and its label below the model name.
class InfoRenderer {
public:
    InfoRenderer();
    ~InfoRenderer();

    /// Load the label font. Call after InitWindow().
    /// If font_path is empty, a system font is discovered automatically.
    void load(const std::string& font_path = "");

    /// Draw the context label and the green→yellow→red health bar.
    /// ratio should be in [0, 1] (clamped internally).
    /// When token_limit > 0, shows "45k / 200k" instead of "Context".
    void draw(float context_ratio, size_t current_tokens = 0, size_t token_limit = 0) const;

private:
    Font m_label_font{};  // "Context" label (10pt)
    bool m_font_loaded = false;
};
