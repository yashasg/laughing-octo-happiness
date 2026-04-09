#if defined(__APPLE__)

#include "platform.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace platform {

bool set_env(const char* name, const char* value) {
    return ::setenv(name, value, 0) == 0;
}

std::string home_dir() {
    if (const char* home = std::getenv("HOME"); home && *home)
        return home;
    return {};
}

std::filesystem::path real_path(const std::filesystem::path& p) {
    return std::filesystem::canonical(p);
}

std::string find_system_font() {
    static const char* candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Geneva.ttf",
        nullptr
    };
    for (const char** c = candidates; *c; ++c) {
        if (std::filesystem::exists(*c)) return *c;
    }
    return {};
}

} // namespace platform

#endif // __APPLE__
