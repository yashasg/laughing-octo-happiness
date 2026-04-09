#pragma once

#include <filesystem>
#include <string>

/// Platform abstraction layer.
/// All platform-specific implementations live in platform_windows.cpp,
/// platform_linux.cpp, and platform_macos.cpp.  The rest of the codebase
/// calls only these portable declarations.
namespace platform {

/// Set an environment variable in the current process.
/// Returns true on success, false on failure.
bool set_env(const char* name, const char* value);

/// Resolve the user's home directory portably.
/// Returns an empty string if the home directory cannot be determined.
std::string home_dir();

/// Resolve the real (canonicalized, junction/symlink-resolved) path.
/// Falls back to std::filesystem::canonical() on platforms that do not
/// need special handling.
std::filesystem::path real_path(const std::filesystem::path& p);

/// Discover a suitable system font for the current platform.
/// Returns an empty string if no suitable font is found.
std::string find_system_font();

} // namespace platform
