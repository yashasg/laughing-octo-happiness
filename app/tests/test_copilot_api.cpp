#include <gtest/gtest.h>
#include "copilot_api.h"
#include "config.h"

// ---------------------------------------------------------------------------
// parse_copilot_models_response — empty / invalid input
// ---------------------------------------------------------------------------
TEST(ParseCopilotModelsResponse, EmptyInputReturnsEmpty) {
    EXPECT_TRUE(parse_copilot_models_response("").empty());
}

TEST(ParseCopilotModelsResponse, InvalidJsonReturnsEmpty) {
    EXPECT_TRUE(parse_copilot_models_response("{not valid}").empty());
}

TEST(ParseCopilotModelsResponse, MissingDataAndModelsKeysReturnsEmpty) {
    EXPECT_TRUE(parse_copilot_models_response(R"({"other": []})").empty());
}

TEST(ParseCopilotModelsResponse, EmptyDataArrayReturnsEmpty) {
    EXPECT_TRUE(parse_copilot_models_response(R"({"data": []})").empty());
}

// ---------------------------------------------------------------------------
// parse_copilot_models_response — OpenAI "data" array format
// ---------------------------------------------------------------------------
TEST(ParseCopilotModelsResponse, OpenAiDataFormatContextWindow) {
    std::string body = R"({
        "data": [
            {"id": "gpt-4o",          "context_window": 128000},
            {"id": "claude-opus-4.5", "context_window": 200000}
        ]
    })";
    auto m = parse_copilot_models_response(body);
    ASSERT_EQ(m.size(), 2u);
    EXPECT_EQ(m.at("gpt-4o"), 128000u);
    EXPECT_EQ(m.at("claude-opus-4.5"), 200000u);
}

TEST(ParseCopilotModelsResponse, CapabilitiesLimitsFormat) {
    std::string body = R"({
        "data": [{
            "id": "gpt-4o",
            "capabilities": {
                "limits": {"max_context_window_tokens": 128000}
            }
        }]
    })";
    auto m = parse_copilot_models_response(body);
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m.at("gpt-4o"), 128000u);
}

TEST(ParseCopilotModelsResponse, DirectContextWindowTakesPrecedenceOverCapabilities) {
    std::string body = R"({
        "data": [{
            "id": "model-x",
            "context_window": 32000,
            "capabilities": {
                "limits": {"max_context_window_tokens": 64000}
            }
        }]
    })";
    auto m = parse_copilot_models_response(body);
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m.at("model-x"), 32000u);
}

// ---------------------------------------------------------------------------
// parse_copilot_models_response — "models" array key
// ---------------------------------------------------------------------------
TEST(ParseCopilotModelsResponse, ModelsArrayKey) {
    std::string body = R"({"models": [{"id": "some-model", "context_window": 64000}]})";
    auto m = parse_copilot_models_response(body);
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m.at("some-model"), 64000u);
}

// ---------------------------------------------------------------------------
// parse_copilot_models_response — robustness
// ---------------------------------------------------------------------------
TEST(ParseCopilotModelsResponse, SkipsEntriesWithoutId) {
    std::string body = R"({"data": [{"context_window": 128000}]})";
    EXPECT_TRUE(parse_copilot_models_response(body).empty());
}

TEST(ParseCopilotModelsResponse, SkipsEntriesWithZeroContextWindow) {
    std::string body = R"({"data": [{"id": "x", "context_window": 0}]})";
    EXPECT_TRUE(parse_copilot_models_response(body).empty());
}

TEST(ParseCopilotModelsResponse, SkipsEntriesWithoutContextWindow) {
    std::string body = R"({"data": [
        {"id": "has-window", "context_window": 128000},
        {"id": "no-window"}
    ]})";
    auto m = parse_copilot_models_response(body);
    ASSERT_EQ(m.size(), 1u);
    EXPECT_TRUE(m.count("has-window"));
    EXPECT_FALSE(m.count("no-window"));
}

TEST(ParseCopilotModelsResponse, MultipleModelsAllParsed) {
    std::string body = R"({
        "data": [
            {"id": "a", "context_window": 10000},
            {"id": "b", "context_window": 20000},
            {"id": "c", "context_window": 30000}
        ]
    })";
    auto m = parse_copilot_models_response(body);
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m.at("a"), 10000u);
    EXPECT_EQ(m.at("b"), 20000u);
    EXPECT_EQ(m.at("c"), 30000u);
}

// ---------------------------------------------------------------------------
// model_context_limit overload with dynamic map
// ---------------------------------------------------------------------------
TEST(ModelContextLimitDynamic, EmptyMapFallsBackToHardcoded) {
    std::unordered_map<std::string, size_t> empty;
    EXPECT_EQ(model_context_limit("gpt-4o", empty), 128000u);
    EXPECT_EQ(model_context_limit("claude-sonnet-4.5", empty), 200000u);
}

TEST(ModelContextLimitDynamic, ExactMatchPrefersDynamicMap) {
    std::unordered_map<std::string, size_t> overrides = {{"gpt-4o", 256000}};
    EXPECT_EQ(model_context_limit("gpt-4o", overrides), 256000u);
}

TEST(ModelContextLimitDynamic, UnknownModelInMapIsFound) {
    std::unordered_map<std::string, size_t> overrides = {{"new-model-2026", 512000}};
    EXPECT_EQ(model_context_limit("new-model-2026", overrides), 512000u);
}

TEST(ModelContextLimitDynamic, UnknownModelNotInMapFallsBackToDefault) {
    std::unordered_map<std::string, size_t> overrides = {{"other-model", 64000}};
    EXPECT_EQ(model_context_limit("completely-unknown", overrides), DEFAULT_CONTEXT_LIMIT);
}

TEST(ModelContextLimitDynamic, DynamicDoesNotAffectPatternMatchForOtherModels) {
    // dynamic map only overrides an exact match, not pattern-matched entries
    std::unordered_map<std::string, size_t> overrides = {{"gpt-4o", 999999}};
    // "claude-sonnet" is pattern-matched from hardcoded, not affected by override
    EXPECT_EQ(model_context_limit("claude-sonnet-4.6", overrides), 200000u);
}
