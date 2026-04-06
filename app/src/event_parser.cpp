#include "event_parser.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOGDI
#  define NOUSER
#  include <windows.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Resolve the real path, following symlinks AND junctions.
// MinGW's fs::canonical() uses _fullpath() which does NOT resolve reparse
// points (junctions/symlinks). On Windows we need GetFinalPathNameByHandleW.
// ---------------------------------------------------------------------------
static fs::path real_path(const fs::path& p) {
#ifdef _WIN32
    HANDLE h = CreateFileW(p.wstring().c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) return fs::canonical(p);

    wchar_t buf[MAX_PATH + 1];
    DWORD len = GetFinalPathNameByHandleW(h, buf, MAX_PATH, FILE_NAME_NORMALIZED);
    CloseHandle(h);

    if (len == 0 || len > MAX_PATH) return fs::canonical(p);

    // Strip the "\\?\" prefix that GetFinalPathNameByHandleW prepends
    std::wstring result(buf, len);
    if (result.size() >= 4 && result.substr(0, 4) == L"\\\\?\\")
        result = result.substr(4);
    return fs::path(result);
#else
    return fs::canonical(p);
#endif
}

// Strip non-printable chars and enforce max length (S3: sanitize untrusted strings)
static std::string sanitize_string(const std::string& input, size_t max_len = 200) {
    std::string out;
    out.reserve(std::min(input.size(), max_len));
    for (size_t i = 0; i < input.size() && out.size() < max_len; ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c >= 0x20 && c != 0x7F) {
            out.push_back(static_cast<char>(c));
        } else if (c == '\t') {
            out.push_back(' ');
        }
    }
    return out;
}

static const std::unordered_map<std::string, CopilotStatus> STATUS_MAP = {
    {"assistant.turn_start",  CopilotStatus::BUSY},
    {"tool.execution_start",  CopilotStatus::BUSY},
    {"assistant.message",     CopilotStatus::BUSY},
    {"hook.start",            CopilotStatus::BUSY},
    {"assistant.turn_end",    CopilotStatus::IDLE},
    {"session.task_complete", CopilotStatus::IDLE},
};

// Tools whose execution_start means the CLI is waiting for user input.
static bool is_waiting_tool(const std::string& tool_name) {
    return tool_name == "exit_plan_mode" || tool_name == "ask_user";
}

ParseResult parse_events(const std::vector<std::string>& lines) {
    ParseResult result;
    bool found_status      = false;
    bool found_text        = false;
    bool found_idle_text   = false;
    bool found_compaction  = false;

    // Tracks whether we've passed the compaction event (reverse scan)
    // so we only accumulate outputTokens for messages BEFORE it (i.e., newer).
    bool past_compaction = false;

    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        const std::string& line = *it;
        if (line.empty()) continue;

        json event;
        try { event = json::parse(line); } catch (...) { continue; }

        const std::string event_type = event.value("type", std::string{});
        const json        data       = event.value("data", json::object());

        // --- Status mapping ---
        if (!found_status) {
            // Special case: tool.execution_start with user-input tools → WAITING
            if (event_type == "tool.execution_start") {
                std::string tool = data.value("toolName", std::string{});
                if (is_waiting_tool(tool)) {
                    result.status = CopilotStatus::WAITING;
                    found_status  = true;
                } else {
                    result.status = CopilotStatus::BUSY;
                    found_status  = true;
                }
            } else {
                auto sit = STATUS_MAP.find(event_type);
                if (sit != STATUS_MAP.end()) {
                    result.status = sit->second;
                    found_status  = true;
                }
            }
        }

        // --- Intent text from tool.execution_start (report_intent only) ---
        if (!found_text && event_type == "tool.execution_start") {
            if (data.value("toolName", std::string{}) == "report_intent") {
                auto args = data.value("arguments", json::object());
                std::string intent = args.value("intent", std::string{});
                if (!intent.empty()) {
                    result.status_text = sanitize_string(intent);
                    found_text         = true;
                }
            }
        }

        // --- outputTokens from assistant.message ---
        if (event_type == "assistant.message") {
            // Accumulate outputTokens for messages before the compaction event
            if (!past_compaction) {
                auto ot = data.value("outputTokens", 0);
                if (ot > 0) result.output_tokens_since += static_cast<size_t>(ot);
            }
        }

        // --- Idle text from session.task_complete ---
        if (!found_idle_text && event_type == "session.task_complete") {
            std::string summary = data.value("summary", std::string{});
            if (!summary.empty()) {
                result.idle_text   = sanitize_string(summary);
                found_idle_text    = true;
            }
        }

        // --- Model name: session.model_change has highest priority ---
        if (result.model_name.empty()) {
            if (event_type == "session.model_change") {
                result.model_name = sanitize_string(data.value("newModel", std::string{}));
            } else if (event_type == "session.start") {
                result.model_name = sanitize_string(data.value("selectedModel", std::string{}));
            } else if (event_type == "tool.execution_complete") {
                std::string model = data.value("model", std::string{});
                if (!model.empty()) result.model_name = sanitize_string(model);
            }
        }

        // --- Token baseline from compaction ---
        if (!found_compaction && event_type == "session.compaction_complete") {
            auto pre = data.value("preCompactionTokens", 0);
            if (pre > 0) {
                result.current_tokens = static_cast<size_t>(pre);
                found_compaction = true;
                past_compaction  = true;
            }
        }

        // Early exit when all fields are populated
        if (found_status && found_text && found_idle_text
            && !result.model_name.empty() && found_compaction) break;
    }

    return result;
}

std::string find_active_session(const std::string& state_dir) {
    std::string best;
    std::filesystem::file_time_type best_mtime{};

    try {
        if (!fs::exists(state_dir)) return {};

        for (const auto& entry : fs::directory_iterator(state_dir)) {
            try {
                if (!entry.is_directory()) continue;

                bool has_lock = false;
                for (const auto& sub : fs::directory_iterator(entry.path())) {
                    const std::string fname = sub.path().filename().string();
                    // Pattern: "inuse." + at least one char + ".lock"
                    if (fname.size() >= 12 &&
                        fname.rfind("inuse.", 0) == 0 &&
                        fname.size() > 6 + 5 &&
                        fname.substr(fname.size() - 5) == ".lock") {
                        has_lock = true;
                        break;
                    }
                }
                if (!has_lock) continue;

                // Validate path stays under state_dir (S1: symlink/junction defense)
                try {
                    fs::path resolved_path  = real_path(entry.path());
                    fs::path resolved_state = real_path(state_dir);
                    auto rel = resolved_path.lexically_relative(resolved_state);
                    if (rel.empty() || rel.string().find("..") == 0) continue;
                } catch (...) { continue; }

                fs::path events_file = entry.path() / "events.jsonl";
                if (!fs::exists(events_file)) continue;

                // Validate events file path stays under state_dir (S1)
                try {
                    fs::path resolved_events = real_path(events_file);
                    fs::path resolved_state  = real_path(state_dir);
                    auto rel = resolved_events.lexically_relative(resolved_state);
                    if (rel.empty() || rel.string().find("..") == 0) continue;
                } catch (...) { continue; }

                auto mtime = fs::last_write_time(events_file);
                if (best.empty() || mtime > best_mtime) {
                    best_mtime = mtime;
                    best = entry.path().string();
                }
            } catch (...) { continue; }
        }
    } catch (...) { return {}; }

    return best;
}
