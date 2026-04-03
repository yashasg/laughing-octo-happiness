#include "event_parser.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

static const std::unordered_map<std::string, CopilotStatus> STATUS_MAP = {
    {"assistant.turn_start",  CopilotStatus::BUSY},
    {"tool.execution_start",  CopilotStatus::BUSY},
    {"assistant.message",     CopilotStatus::BUSY},
    {"hook.start",            CopilotStatus::BUSY},
    {"assistant.turn_end",    CopilotStatus::WAITING},
    {"session.task_complete", CopilotStatus::IDLE},
};

ParseResult parse_events(const std::vector<std::string>& lines) {
    ParseResult result;
    bool found_status = false;
    bool found_text   = false;

    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        const std::string& line = *it;
        if (line.empty()) continue;

        json event;
        try { event = json::parse(line); } catch (...) { continue; }

        const std::string event_type = event.value("type", std::string{});
        const json        data       = event.value("data", json::object());

        if (!found_status) {
            auto sit = STATUS_MAP.find(event_type);
            if (sit != STATUS_MAP.end()) {
                result.status = sit->second;
                found_status  = true;
            }
        }

        if (!found_text && event_type == "tool.execution_start") {
            if (data.value("toolName", std::string{}) == "report_intent") {
                auto args = data.value("arguments", json::object());
                std::string intent = args.value("intent", std::string{});
                if (!intent.empty()) {
                    result.status_text = std::move(intent);
                    found_text         = true;
                }
            }
        }

        if (result.model_name.empty()) {
            if (event_type == "session.start") {
                result.model_name = data.value("selectedModel", std::string{});
            } else if (event_type == "tool.execution_complete") {
                std::string model = data.value("model", std::string{});
                if (!model.empty()) result.model_name = std::move(model);
            }
        }

        if (found_status && found_text && !result.model_name.empty()) break;
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

                fs::path events_file = entry.path() / "events.jsonl";
                if (!fs::exists(events_file)) continue;

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
