#pragma once

#include "platform.h"
#include <string>

/// Discover a suitable system font for the current platform.
/// Returns an empty string if no suitable font is found.
inline std::string find_system_font() {
    return platform::find_system_font();
}
