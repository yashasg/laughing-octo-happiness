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
    bool found_status    = false;
    bool found_text      = false;
    bool found_reasoning = false;
    bool found_compaction = false;

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
            auto sit = STATUS_MAP.find(event_type);
            if (sit != STATUS_MAP.end()) {
                result.status = sit->second;
                found_status  = true;
            }
        }

        // --- Intent text from tool.execution_start (report_intent) ---
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

        // --- Intent + reasoningText + outputTokens from assistant.message ---
        if (event_type == "assistant.message") {
            // Extract intent from toolRequests[].name == "report_intent"
            if (!found_text && data.contains("toolRequests") && data["toolRequests"].is_array()) {
                for (const auto& req : data["toolRequests"]) {
                    if (req.value("name", std::string{}) == "report_intent") {
                        auto args = req.value("arguments", json::object());
                        std::string intent = args.value("intent", std::string{});
                        if (!intent.empty()) {
                            result.status_text = std::move(intent);
                            found_text         = true;
                            break;
                        }
                    }
                }
            }

            // Use reasoningText as fallback status text
            if (!found_text && !found_reasoning) {
                std::string reasoning = data.value("reasoningText", std::string{});
                if (!reasoning.empty()) {
                    result.status_text = std::move(reasoning);
                    found_reasoning    = true;
                }
            }

            // Accumulate outputTokens for messages before the compaction event
            if (!past_compaction) {
                auto ot = data.value("outputTokens", 0);
                if (ot > 0) result.output_tokens_since += static_cast<size_t>(ot);
            }
        }

        // --- Model name: session.model_change has highest priority ---
        if (result.model_name.empty()) {
            if (event_type == "session.model_change") {
                result.model_name = data.value("newModel", std::string{});
            } else if (event_type == "session.start") {
                result.model_name = data.value("selectedModel", std::string{});
            } else if (event_type == "tool.execution_complete") {
                std::string model = data.value("model", std::string{});
                if (!model.empty()) result.model_name = std::move(model);
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
        if (found_status && (found_text || found_reasoning)
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
