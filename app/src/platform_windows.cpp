#ifdef _WIN32

#include "platform.h"

#include <cstdlib>
#include <filesystem>
#include <string>

// Forward-declare MSG/LPMSG before including windows.h.
// With NOUSER defined (via CMake), winuser.h is excluded so LPMSG is never
// defined; however ole2.h → oleidl.h still references it.  The forward
// declarations satisfy those references without pulling in winuser.h.
struct tagMSG;
typedef struct tagMSG MSG;
typedef MSG *LPMSG;

#include <windows.h>

namespace platform {

bool set_env(const char* name, const char* value) {
    return _putenv_s(name, value) == 0;
}

std::string home_dir() {
    if (const char* up = std::getenv("USERPROFILE"); up && *up)
        return up;
    const char* hd = std::getenv("HOMEDRIVE");
    const char* hp = std::getenv("HOMEPATH");
    if (hd && *hd && hp && *hp)
        return std::string(hd) + hp;
    return {};
}

// MinGW's fs::canonical() uses _fullpath() which does NOT resolve reparse
// points (junctions/symlinks).  Use GetFinalPathNameByHandleW instead.
std::filesystem::path real_path(const std::filesystem::path& p) {
    HANDLE h = CreateFileW(p.wstring().c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::filesystem::canonical(p);

    wchar_t buf[MAX_PATH + 1];
    DWORD len = GetFinalPathNameByHandleW(h, buf, MAX_PATH, FILE_NAME_NORMALIZED);
    CloseHandle(h);

    if (len == 0 || len > MAX_PATH) return std::filesystem::canonical(p);

    // Strip the "\\?\" prefix that GetFinalPathNameByHandleW prepends.
    std::wstring result(buf, len);
    if (result.size() >= 4 && result.substr(0, 4) == L"\\\\?\\")
        result = result.substr(4);
    return std::filesystem::path(result);
}

std::string find_system_font() {
    static const char* candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/verdana.ttf",
        nullptr
    };
    for (const char** c = candidates; *c; ++c) {
        if (std::filesystem::exists(*c)) return *c;
    }
    return {};
}

} // namespace platform

#endif // _WIN32
