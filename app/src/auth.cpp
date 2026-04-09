#include "auth.h"
#include "platform.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

void ensure_github_token() {
    if (std::getenv("COPILOT_GITHUB_TOKEN")) return;
    if (std::getenv("GH_TOKEN"))             return;
    if (std::getenv("GITHUB_TOKEN"))         return;

    FILE* pipe = popen("gh auth token 2>/dev/null", "r");
    if (!pipe) return;

    char buf[256]{};
    bool has_output = (fgets(buf, sizeof(buf), pipe) != nullptr);
    int exit_status = pclose(pipe);

    if (!has_output || exit_status != 0) return;

    std::string token(buf);
    while (!token.empty() && (token.back() == '\n' || token.back() == '\r' || token.back() == ' '))
        token.pop_back();
    if (!token.empty()) {
        platform::set_env("COPILOT_GITHUB_TOKEN", token.c_str());
        std::cout << "[auth] Token acquired from gh CLI\n";
    }
}
