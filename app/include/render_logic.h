#pragma once

#include "config.h"
#include "raylib.h"  // for Color type (just a struct, no GPU calls)
#include <string>
#include <algorithm>

/// Truncate text to max_len chars using center-ellipsis: "head...tail".
/// Preserves the beginning (what) and end (target) of longer strings.
inline std::string truncate_text(const std::string& text, int max_len) {
    if (max_len < 7) max_len = 7;  // minimum for "a...b"
    if (static_cast<int>(text.size()) <= max_len) return text;
    int keep = max_len - 3;          // chars to keep (minus "...")
    int head = (keep + 1) / 2;       // slightly more on the left
    int tail = keep - head;
    return text.substr(0, head) + "..." + text.substr(text.size() - tail);
}

/// Compute the health bar fill color for the given ratio [0, 1].
/// Green (low) → yellow (mid) → red (high). Alpha fixed at 230.
inline Color compute_bar_color(float ratio) {
    ratio = std::max(0.0f, std::min(1.0f, ratio));
    unsigned char r, g;
    const unsigned char b = 60;
    if (ratio < 0.5f) {
        r = static_cast<unsigned char>(80.0f + 175.0f * (ratio / 0.5f));
        g = 200;
    } else {
        r = 255;
        g = static_cast<unsigned char>(200.0f * (1.0f - (ratio - 0.5f) / 0.5f));
    }
    return { r, g, b, 230 };
}

/// Compute the health bar fill width for the given ratio [0, 1].
/// Result is clamped to a minimum of 2 * BAR_RADIUS.
inline float compute_bar_fill_width(float ratio, float bar_width) {
    ratio = std::max(0.0f, std::min(1.0f, ratio));
    return std::max(2.0f * BAR_RADIUS, bar_width * ratio);
}
