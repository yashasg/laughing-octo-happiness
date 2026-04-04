#pragma once

#include "config.h"
#include <string>
#include <vector>

/// Result of parsing a set of JSONL event lines.
struct ParseResult {
    CopilotStatus status     = CopilotStatus::IDLE;
    std::string   status_text;   ///< intent or reasoningText from recent events
    std::string   model_name;    ///< model from session.model_change, session.start, or tool.execution_complete

    // Token metrics (from persisted events)
    size_t        current_tokens     = 0;  ///< from session.compaction_complete preCompactionTokens
    size_t        output_tokens_since = 0;  ///< accumulated outputTokens from assistant.message after compaction
};

/// Walk lines in reverse order and derive the current status, status text,
/// and model name. Lines must already be split; the caller is responsible for
/// discarding any partial leading line when tail-reading a file.
ParseResult parse_events(const std::vector<std::string>& lines);

/// Scan state_dir for the active session subdirectory.
/// The active session has an inuse.*.lock file and an events.jsonl.
/// When multiple candidates exist the one with the most-recent events.jsonl
/// mtime is returned. Returns an empty string if none is found.
std::string find_active_session(const std::string& state_dir);
