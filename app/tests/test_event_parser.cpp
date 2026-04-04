#include <gtest/gtest.h>
#include "event_parser.h"

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
