#include <gtest/gtest.h>
#include "event_parser.h"
#include <nlohmann/json.hpp>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string event(const std::string& type) {
    return R"({"type":")" + type + R"("})";
}

static std::string event_with_data(const std::string& type, const std::string& data_json) {
    return R"({"type":")" + type + R"(","data":)" + data_json + "}";
}

// ---------------------------------------------------------------------------
// Empty / trivial
// ---------------------------------------------------------------------------
TEST(ParseEvents, EmptyLines) {
    ParseResult r = parse_events({});
    EXPECT_EQ(r.status, CopilotStatus::IDLE);
    EXPECT_TRUE(r.status_text.empty());
    EXPECT_TRUE(r.model_name.empty());
}

TEST(ParseEvents, BlankLinesOnly) {
    ParseResult r = parse_events({"", "  ", ""});
    EXPECT_EQ(r.status, CopilotStatus::IDLE);
}

TEST(ParseEvents, InvalidJsonOnly) {
    ParseResult r = parse_events({"not json", "{bad", "}"});
    EXPECT_EQ(r.status, CopilotStatus::IDLE);
}

TEST(ParseEvents, UnknownEventType) {
    ParseResult r = parse_events({event("some.unknown.event")});
    EXPECT_EQ(r.status, CopilotStatus::IDLE);
}

// ---------------------------------------------------------------------------
// Status mapping
// ---------------------------------------------------------------------------
TEST(ParseEvents, BusyOnAssistantTurnStart) {
    EXPECT_EQ(parse_events({event("assistant.turn_start")}).status, CopilotStatus::BUSY);
}

TEST(ParseEvents, BusyOnToolExecutionStart) {
    EXPECT_EQ(parse_events({event_with_data("tool.execution_start",
        R"({"toolName":"bash"})")}).status, CopilotStatus::BUSY);
}

TEST(ParseEvents, BusyOnAssistantMessage) {
    EXPECT_EQ(parse_events({event("assistant.message")}).status, CopilotStatus::BUSY);
}

TEST(ParseEvents, BusyOnHookStart) {
    EXPECT_EQ(parse_events({event("hook.start")}).status, CopilotStatus::BUSY);
}

TEST(ParseEvents, IdleOnAssistantTurnEnd) {
    EXPECT_EQ(parse_events({event("assistant.turn_end")}).status, CopilotStatus::IDLE);
}

TEST(ParseEvents, IdleOnSessionTaskComplete) {
    EXPECT_EQ(parse_events({event("session.task_complete")}).status, CopilotStatus::IDLE);
}

// ---------------------------------------------------------------------------
// Last event wins (reverse-walk)
// ---------------------------------------------------------------------------
TEST(ParseEvents, LastEventWins_BusyThenIdle) {
    // Chronological order: turn_start → turn_end
    // Reverse-walk finds turn_end first → IDLE
    ParseResult r = parse_events({
        event("assistant.turn_start"),
        event("assistant.turn_end"),
    });
    EXPECT_EQ(r.status, CopilotStatus::IDLE);
}

TEST(ParseEvents, LastEventWins_WaitingThenBusy) {
    ParseResult r = parse_events({
        event("assistant.turn_end"),
        event("assistant.turn_start"),
    });
    EXPECT_EQ(r.status, CopilotStatus::BUSY);
}

// ---------------------------------------------------------------------------
// Intent / status_text extraction
// ---------------------------------------------------------------------------
TEST(ParseEvents, ExtractsIntentFromReportIntent) {
    std::string line = event_with_data("tool.execution_start",
        R"({"toolName":"report_intent","arguments":{"intent":"Fixing the bug"}})");
    ParseResult r = parse_events({line});
    EXPECT_EQ(r.status_text, "Fixing the bug");
}

TEST(ParseEvents, IgnoresIntentFromOtherTools) {
    std::string line = event_with_data("tool.execution_start",
        R"({"toolName":"bash","arguments":{"intent":"should not appear"}})");
    EXPECT_TRUE(parse_events({line}).status_text.empty());
}

TEST(ParseEvents, EmptyIntentIsIgnored) {
    std::string line = event_with_data("tool.execution_start",
        R"({"toolName":"report_intent","arguments":{"intent":""}})");
    EXPECT_TRUE(parse_events({line}).status_text.empty());
}

TEST(ParseEvents, MostRecentIntentWins) {
    std::string old_intent = event_with_data("tool.execution_start",
        R"({"toolName":"report_intent","arguments":{"intent":"Old intent"}})");
    std::string new_intent = event_with_data("tool.execution_start",
        R"({"toolName":"report_intent","arguments":{"intent":"New intent"}})");
    // Reverse-walk: new_intent (last) is found first
    ParseResult r = parse_events({old_intent, new_intent});
    EXPECT_EQ(r.status_text, "New intent");
}

// ---------------------------------------------------------------------------
// Model name extraction
// ---------------------------------------------------------------------------
TEST(ParseEvents, ExtractsModelFromSessionStart) {
    std::string line = event_with_data("session.start",
        R"({"selectedModel":"claude-sonnet-4.6"})");
    EXPECT_EQ(parse_events({line}).model_name, "claude-sonnet-4.6");
}

TEST(ParseEvents, ExtractsModelFromToolExecutionComplete) {
    std::string line = event_with_data("tool.execution_complete",
        R"({"model":"gpt-4"})");
    EXPECT_EQ(parse_events({line}).model_name, "gpt-4");
}

TEST(ParseEvents, EmptyModelInToolExecutionCompleteIsIgnored) {
    std::string line = event_with_data("tool.execution_complete", R"({"model":""})");
    EXPECT_TRUE(parse_events({line}).model_name.empty());
}

TEST(ParseEvents, MissingModelFieldIsEmpty) {
    ParseResult r = parse_events({event("tool.execution_complete")});
    EXPECT_TRUE(r.model_name.empty());
}

// ---------------------------------------------------------------------------
// Mixed scenarios
// ---------------------------------------------------------------------------
TEST(ParseEvents, FullSession) {
    std::vector<std::string> lines = {
        event_with_data("session.start", R"({"selectedModel":"claude-sonnet-4.6"})"),
        event("assistant.turn_start"),
        event_with_data("tool.execution_start",
            R"({"toolName":"report_intent","arguments":{"intent":"Working on it"}})"),
        event("assistant.turn_end"),
    };
    ParseResult r = parse_events(lines);
    EXPECT_EQ(r.status, CopilotStatus::IDLE);  // turn_end → IDLE
    EXPECT_EQ(r.status_text, "Working on it");
    EXPECT_EQ(r.model_name, "claude-sonnet-4.6");
}

TEST(ParseEvents, SkipsMalformedLinesAndContinues) {
    ParseResult r = parse_events({
        "not json",
        event("assistant.turn_end"),
        "also not json",
    });
    EXPECT_EQ(r.status, CopilotStatus::IDLE);
}

// ---------------------------------------------------------------------------
// Intent ONLY from tool.execution_start (report_intent)
// assistant.message toolRequests and reasoningText are NOT used for status_text
// ---------------------------------------------------------------------------
TEST(ParseEvents, NoIntentFromAssistantMessageToolRequests) {
    // report_intent in assistant.message toolRequests should NOT set status_text
    std::string line = event_with_data("assistant.message",
        R"({"toolRequests":[{"name":"report_intent","arguments":{"intent":"Exploring codebase"}}],"outputTokens":100})");
    ParseResult r = parse_events({line});
    EXPECT_TRUE(r.status_text.empty());
}

TEST(ParseEvents, IntentFromToolExecutionStartOnly) {
    // tool.execution_start report_intent sets status_text
    std::string msg = event_with_data("assistant.message",
        R"({"toolRequests":[{"name":"report_intent","arguments":{"intent":"Old intent"}}],"outputTokens":50})");
    std::string exec = event_with_data("tool.execution_start",
        R"({"toolName":"report_intent","arguments":{"intent":"New intent"}})");
    ParseResult r = parse_events({msg, exec});
    EXPECT_EQ(r.status_text, "New intent");
}

TEST(ParseEvents, ReasoningTextNotUsedForStatusText) {
    std::string line = event_with_data("assistant.message",
        R"({"reasoningText":"Let me analyze the code","outputTokens":200})");
    ParseResult r = parse_events({line});
    EXPECT_TRUE(r.status_text.empty());
}

// ---------------------------------------------------------------------------
// Model name from session.model_change
// ---------------------------------------------------------------------------
TEST(ParseEvents, ExtractsModelFromModelChange) {
    std::string line = event_with_data("session.model_change",
        R"({"newModel":"claude-opus-4.6-1m","previousModel":"claude-sonnet-4.6"})");
    EXPECT_EQ(parse_events({line}).model_name, "claude-opus-4.6-1m");
}

TEST(ParseEvents, ModelChangeOverridesSessionStart) {
    std::vector<std::string> lines = {
        event_with_data("session.start", R"({"selectedModel":"claude-sonnet-4.6"})"),
        event_with_data("session.model_change", R"({"newModel":"claude-opus-4.6-1m"})"),
    };
    // Reverse-walk: model_change is found first
    EXPECT_EQ(parse_events(lines).model_name, "claude-opus-4.6-1m");
}

// ---------------------------------------------------------------------------
// Token metrics
// ---------------------------------------------------------------------------
TEST(ParseEvents, ExtractsTokensFromCompaction) {
    std::string line = event_with_data("session.compaction_complete",
        R"({"success":true,"preCompactionTokens":135427})");
    ParseResult r = parse_events({line});
    EXPECT_EQ(r.current_tokens, 135427u);
}

TEST(ParseEvents, AccumulatesOutputTokensBeforeCompaction) {
    std::vector<std::string> lines = {
        event_with_data("session.compaction_complete",
            R"({"success":true,"preCompactionTokens":100000})"),
        event_with_data("assistant.message", R"({"outputTokens":200})"),
        event_with_data("assistant.message", R"({"outputTokens":300})"),
    };
    ParseResult r = parse_events(lines);
    EXPECT_EQ(r.current_tokens, 100000u);
    EXPECT_EQ(r.output_tokens_since, 500u);  // 200 + 300 after compaction
}

TEST(ParseEvents, NoTokenDataWhenNoCompaction) {
    std::vector<std::string> lines = {
        event_with_data("assistant.message", R"({"outputTokens":500})"),
    };
    ParseResult r = parse_events(lines);
    EXPECT_EQ(r.current_tokens, 0u);
    EXPECT_EQ(r.output_tokens_since, 500u);
}

// ---------------------------------------------------------------------------
// WAITING state from user-input tools
// ---------------------------------------------------------------------------
TEST(ParseEvents, WaitingOnExitPlanMode) {
    std::string line = event_with_data("tool.execution_start",
        R"({"toolName":"exit_plan_mode","arguments":{}})");
    EXPECT_EQ(parse_events({line}).status, CopilotStatus::WAITING);
}

TEST(ParseEvents, WaitingOnAskUser) {
    std::string line = event_with_data("tool.execution_start",
        R"({"toolName":"ask_user","arguments":{"question":"Which DB?"}})");
    EXPECT_EQ(parse_events({line}).status, CopilotStatus::WAITING);
}

TEST(ParseEvents, BusyOnNormalTool) {
    std::string line = event_with_data("tool.execution_start",
        R"({"toolName":"bash","arguments":{"command":"ls"}})");
    EXPECT_EQ(parse_events({line}).status, CopilotStatus::BUSY);
}

// ---------------------------------------------------------------------------
// intentionSummary extraction
// ---------------------------------------------------------------------------
TEST(ParseEvents, IntentionSummaryNotUsedForStatusText) {
    std::string line = event_with_data("assistant.message",
        R"({"toolRequests":[{"name":"bash","intentionSummary":"Rebuild the C++ app"}],"outputTokens":50})");
    ParseResult r = parse_events({line});
    EXPECT_TRUE(r.status_text.empty());
}

TEST(ParseEvents, OnlyToolExecutionStartReportIntentSetsStatusText) {
    // assistant.message toolRequests should NOT set status_text, even with report_intent
    std::string line = event_with_data("assistant.message",
        R"({"toolRequests":[{"name":"report_intent","arguments":{"intent":"Fixing bug"}},{"name":"bash","intentionSummary":"Run tests"}],"outputTokens":50})");
    ParseResult r = parse_events({line});
    EXPECT_TRUE(r.status_text.empty());
}

// ---------------------------------------------------------------------------
// idle_text from session.task_complete
// ---------------------------------------------------------------------------
TEST(ParseEvents, ExtractsIdleTextFromTaskComplete) {
    std::string line = event_with_data("session.task_complete",
        R"({"summary":"Fixed the bug in main.cpp"})");
    ParseResult r = parse_events({line});
    EXPECT_EQ(r.idle_text, "Fixed the bug in main.cpp");
}

TEST(ParseEvents, IdleTextEmptyWhenNoTaskComplete) {
    ParseResult r = parse_events({event("assistant.turn_end")});
    EXPECT_TRUE(r.idle_text.empty());
}

TEST(ParseEvents, FullSessionWithIdleText) {
    std::vector<std::string> lines = {
        event_with_data("session.start", R"({"selectedModel":"claude-sonnet-4.6"})"),
        event("assistant.turn_start"),
        event_with_data("tool.execution_start",
            R"({"toolName":"report_intent","arguments":{"intent":"Building project"}})"),
        event_with_data("assistant.message",
            R"({"toolRequests":[{"name":"bash","intentionSummary":"Build the project"}],"outputTokens":100})"),
        event_with_data("session.task_complete",
            R"({"summary":"Built successfully"})"),
    };
    ParseResult r = parse_events(lines);
    EXPECT_EQ(r.status, CopilotStatus::IDLE);
    EXPECT_EQ(r.idle_text, "Built successfully");
    EXPECT_EQ(r.status_text, "Building project");
}

// ---------------------------------------------------------------------------
// Token metrics — advanced scenarios
// ---------------------------------------------------------------------------
TEST(ParseEvents, MultipleCompactionEventsUseMostRecent) {
    // Two compaction events; reverse scan finds the later one first
    std::vector<std::string> lines = {
        event_with_data("session.compaction_complete",
            R"({"preCompactionTokens": 30000})"),
        event_with_data("assistant.message", R"({"outputTokens": 100})"),
        event_with_data("session.compaction_complete",
            R"({"preCompactionTokens": 60000})"),
        event_with_data("assistant.message", R"({"outputTokens": 200})"),
    };
    ParseResult r = parse_events(lines);
    // Reverse: line[3] msg(200) → line[2] compaction(60k) → stop accumulating
    EXPECT_EQ(r.current_tokens, 60000u);
    EXPECT_EQ(r.output_tokens_since, 200u);
}

TEST(ParseEvents, OutputTokensNotAccumulatedAfterCompaction) {
    // In reverse scan: older messages (below compaction) must NOT accumulate
    std::vector<std::string> lines = {
        event_with_data("assistant.message", R"({"outputTokens": 500})"),
        event_with_data("session.compaction_complete",
            R"({"preCompactionTokens": 40000})"),
        event_with_data("assistant.message", R"({"outputTokens": 300})"),
    };
    ParseResult r = parse_events(lines);
    EXPECT_EQ(r.current_tokens, 40000u);
    EXPECT_EQ(r.output_tokens_since, 300u);  // only the newer message
}

TEST(ParseEvents, CompactionWithZeroTokensIsIgnored) {
    std::vector<std::string> lines = {
        event_with_data("session.compaction_complete",
            R"({"preCompactionTokens": 0})"),
        event_with_data("assistant.message", R"({"outputTokens": 100})"),
    };
    ParseResult r = parse_events(lines);
    EXPECT_EQ(r.current_tokens, 0u);
    // outputTokens still accumulate — no valid compaction was found
    EXPECT_EQ(r.output_tokens_since, 100u);
}

// ---------------------------------------------------------------------------
// Model name — edge cases
// ---------------------------------------------------------------------------
TEST(ParseEvents, ModelChangeWithEmptyNewModelIsIgnored) {
    std::vector<std::string> lines = {
        event_with_data("session.model_change", R"({"newModel": ""})"),
        event_with_data("session.start", R"({"selectedModel": "gpt-4o"})"),
    };
    ParseResult r = parse_events(lines);
    EXPECT_EQ(r.model_name, "gpt-4o");
}

// ---------------------------------------------------------------------------
// Robustness — edge cases
// ---------------------------------------------------------------------------
TEST(ParseEvents, EventWithMissingTypeField) {
    std::vector<std::string> lines = {
        R"({"data": {"toolName": "view"}})",
        event("assistant.turn_start"),
    };
    ParseResult r = parse_events(lines);
    EXPECT_EQ(r.status, CopilotStatus::BUSY);
}

TEST(ParseEvents, EarlyExitWhenAllFieldsPopulated) {
    std::vector<std::string> lines = {
        event_with_data("session.start", R"({"selectedModel": "gpt-5"})"),
        event_with_data("session.compaction_complete",
            R"({"preCompactionTokens": 50000})"),
        event_with_data("session.task_complete",
            R"({"summary": "Done"})"),
        event_with_data("tool.execution_start",
            R"({"toolName": "report_intent", "arguments": {"intent": "Testing"}})"),
        event("assistant.turn_start"),
    };
    ParseResult r = parse_events(lines);
    EXPECT_EQ(r.status, CopilotStatus::BUSY);
    EXPECT_EQ(r.status_text, "Testing");
    EXPECT_EQ(r.idle_text, "Done");
    EXPECT_EQ(r.model_name, "gpt-5");
    EXPECT_EQ(r.current_tokens, 50000u);
}

TEST(ParseEvents, DefaultResultForSingleUnknownEvent) {
    ParseResult r = parse_events({R"({"type":"custom.event","data":{}})"});
    EXPECT_EQ(r.status, CopilotStatus::IDLE);
    EXPECT_TRUE(r.status_text.empty());
    EXPECT_TRUE(r.idle_text.empty());
    EXPECT_TRUE(r.model_name.empty());
    EXPECT_EQ(r.current_tokens, 0u);
    EXPECT_EQ(r.output_tokens_since, 0u);
}

// ---------------------------------------------------------------------------
// Sanitization — control chars must be stripped from extracted strings
// ---------------------------------------------------------------------------
TEST(ParseEvents, SanitizesControlCharsInIntent) {
    // Build JSON with actual control char bytes embedded in the intent value
    std::string intent = std::string("Fix") + '\x01' + '\x02' + " bug";
    // nlohmann/json can serialize control chars → we construct via the library
    nlohmann::json j;
    j["type"] = "tool.execution_start";
    j["data"]["toolName"] = "report_intent";
    j["data"]["arguments"]["intent"] = intent;
    std::string line = j.dump();

    ParseResult r = parse_events({line});
    EXPECT_EQ(r.status_text.find('\x01'), std::string::npos);
    EXPECT_EQ(r.status_text.find('\x02'), std::string::npos);
    EXPECT_NE(r.status_text.find("Fix"), std::string::npos);
    EXPECT_NE(r.status_text.find("bug"), std::string::npos);
}

TEST(ParseEvents, SanitizesControlCharsInModelName) {
    nlohmann::json j;
    j["type"] = "session.start";
    j["data"]["selectedModel"] = std::string("claude") + '\n' + "sneaky";
    ParseResult r = parse_events({j.dump()});
    EXPECT_EQ(r.model_name.find('\n'), std::string::npos);
}

TEST(ParseEvents, SanitizesControlCharsInIdleText) {
    nlohmann::json j;
    j["type"] = "session.task_complete";
    j["data"]["summary"] = std::string("Done") + '\r' + '\n' + "with task";
    ParseResult r = parse_events({j.dump()});
    EXPECT_EQ(r.idle_text.find('\r'), std::string::npos);
    EXPECT_EQ(r.idle_text.find('\n'), std::string::npos);
}

TEST(ParseEvents, SanitizesTabsInIntent) {
    std::string line = event_with_data("tool.execution_start",
        R"({"toolName":"report_intent","arguments":{"intent":"Fix\tbug"}})");
    ParseResult r = parse_events({line});
    // Tabs should be converted to spaces
    EXPECT_EQ(r.status_text.find('\t'), std::string::npos);
    EXPECT_NE(r.status_text.find("Fix"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Edge cases — missing or wrong-typed fields in tool.execution_start
// ---------------------------------------------------------------------------
TEST(ParseEvents, ToolExecutionStartWithMissingToolName) {
    // No toolName field — should default to BUSY (not crash)
    std::string line = event_with_data("tool.execution_start",
        R"({"arguments":{"command":"ls"}})");
    ParseResult r = parse_events({line});
    EXPECT_EQ(r.status, CopilotStatus::BUSY);
}

TEST(ParseEvents, ToolExecutionStartWithNullToolName) {
    // BUG: null toolName causes nlohmann::json::value<string>() to throw
    // type_error because the key exists but can't convert null to string.
    // The exception propagates out of parse_events (no inner catch).
    // This test documents the current behavior; ideally it should map to BUSY.
    std::string line = event_with_data("tool.execution_start",
        R"({"toolName":null})");
    EXPECT_ANY_THROW(parse_events({line}));
}

TEST(ParseEvents, NegativeOutputTokensIgnored) {
    std::vector<std::string> lines = {
        event_with_data("assistant.message", R"({"outputTokens":-500})"),
    };
    ParseResult r = parse_events(lines);
    // Negative tokens should not be accumulated (ot > 0 check)
    EXPECT_EQ(r.output_tokens_since, 0u);
}

TEST(ParseEvents, ZeroOutputTokensIgnored) {
    std::vector<std::string> lines = {
        event_with_data("assistant.message", R"({"outputTokens":0})"),
    };
    ParseResult r = parse_events(lines);
    EXPECT_EQ(r.output_tokens_since, 0u);
}
