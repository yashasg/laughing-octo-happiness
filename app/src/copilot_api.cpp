#include "copilot_api.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <cstdio>
#include <iostream>
#include <string>

using json = nlohmann::json;

// Editor version header sent with every Copilot API request.
static constexpr const char* COPILOT_EDITOR_VERSION = "vscode/1.85.0";

// ---------------------------------------------------------------------------
// parse_copilot_models_response
// ---------------------------------------------------------------------------
std::unordered_map<std::string, size_t>
parse_copilot_models_response(const std::string& json_body) {
    std::unordered_map<std::string, size_t> limits;
    if (json_body.empty()) return limits;

    try {
        auto j = json::parse(json_body);

        // Accept both the OpenAI "data" array format and a plain "models" key.
        json arr;
        if (j.contains("data") && j["data"].is_array())
            arr = j["data"];
        else if (j.contains("models") && j["models"].is_array())
            arr = j["models"];
        else
            return limits;

        for (const auto& model : arr) {
            if (!model.is_object()) continue;

            std::string id = model.value("id", std::string{});
            if (id.empty()) continue;

            size_t ctx = 0;

            // Primary field: top-level context_window (OpenAI models format)
            if (model.contains("context_window") && model["context_window"].is_number())
                ctx = model["context_window"].get<size_t>();

            // Fallback: capabilities.limits.max_context_window_tokens
            if (ctx == 0 && model.contains("capabilities")) {
                const auto& caps = model["capabilities"];
                if (caps.is_object() && caps.contains("limits")) {
                    const auto& lims = caps["limits"];
                    if (lims.is_object() && lims.contains("max_context_window_tokens")
                        && lims["max_context_window_tokens"].is_number())
                        ctx = lims["max_context_window_tokens"].get<size_t>();
                }
            }

            if (ctx > 0) limits[id] = ctx;
        }
    } catch (...) {
        // Malformed JSON or unexpected structure — return whatever was collected.
    }

    return limits;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Returns true iff every character in the token is safe to embed in a
// double-quoted shell argument (no shell metacharacters).
// GitHub tokens use only alphanumeric chars plus '_' and '-'.
static bool is_safe_token(const std::string& token) {
    if (token.empty() || token.size() > 512) return false;
    for (unsigned char c : token) {
        if (!isalnum(c) && c != '_' && c != '-' && c != '.') return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// fetch_copilot_model_limits
// ---------------------------------------------------------------------------
std::unordered_map<std::string, size_t> fetch_copilot_model_limits() {
    // Resolve the GitHub token from the environment (highest → lowest priority).
    const char* raw_token = std::getenv("COPILOT_GITHUB_TOKEN");
    if (!raw_token || raw_token[0] == '\0') raw_token = std::getenv("GH_TOKEN");
    if (!raw_token || raw_token[0] == '\0') raw_token = std::getenv("GITHUB_TOKEN");
    if (!raw_token || raw_token[0] == '\0') return {};

    std::string token(raw_token);
    if (!is_safe_token(token)) {
        std::cerr << "[copilot_api] Token contains unsafe characters; skipping API call.\n";
        return {};
    }

    // JSON-RPC-style GET /models call to the GitHub Copilot API.
    // Uses curl (available on macOS, Linux, and Windows 10+).
    std::string cmd =
        "curl -s --max-time 10 "
        "-H \"Authorization: Bearer " + token + "\" "
        "-H \"Accept: application/json\" "
        "-H \"Editor-Version: " + std::string(COPILOT_EDITOR_VERSION) + "\" "
        "https://api.githubcopilot.com/models 2>/dev/null";

#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return {};

    std::string response;
    response.reserve(65536);
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        response += buf;
        if (response.size() > 1024 * 1024) break;  // 1 MB safety cap
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    auto limits = parse_copilot_models_response(response);
    if (!limits.empty()) {
        std::cout << "[copilot_api] Fetched context limits for "
                  << limits.size() << " models from GitHub Copilot API.\n";
    }
    return limits;
}
