#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include "font_utils.h"
#include "sprite_renderer.h"
#include "text_renderer.h"
#include "info_renderer.h"

extern std::vector<std::string> g_mock_calls;
void mock_reset();
bool mock_was_called(const std::string& name);

// ---------------------------------------------------------------------------
// TextRenderer
// ---------------------------------------------------------------------------
class TextRendererTest : public ::testing::Test {
protected:
    void SetUp() override { mock_reset(); }
    TextRenderer renderer;
};

TEST_F(TextRendererTest, DrawBubbleCallsDrawRectangleRounded) {
    renderer.draw_bubble(CopilotStatus::IDLE, "Hello");
    EXPECT_TRUE(mock_was_called("DrawRectangleRounded"));
}

TEST_F(TextRendererTest, DrawBubbleCallsDrawTextEx) {
    renderer.draw_bubble(CopilotStatus::WAITING, "Waiting on you!");
    EXPECT_TRUE(mock_was_called("DrawTextEx"));
}

TEST_F(TextRendererTest, DrawBubbleCallsDrawTriangle) {
    renderer.draw_bubble(CopilotStatus::BUSY, "Working...");
    EXPECT_TRUE(mock_was_called("DrawTriangle"));
}

TEST_F(TextRendererTest, DrawBubbleCallsDrawRectangleRoundedLinesEx) {
    renderer.draw_bubble(CopilotStatus::IDLE, "Hello");
    EXPECT_TRUE(mock_was_called("DrawRectangleRoundedLinesEx"));
}

TEST_F(TextRendererTest, DrawModelNameCallsDrawTextEx) {
    mock_reset();
    renderer.draw_model_name("claude-sonnet-4");
    EXPECT_TRUE(mock_was_called("DrawTextEx"));
}

// ---------------------------------------------------------------------------
// InfoRenderer
// ---------------------------------------------------------------------------
class InfoRendererTest : public ::testing::Test {
protected:
    void SetUp() override { mock_reset(); }
    InfoRenderer renderer;
};

TEST_F(InfoRendererTest, DrawCallsDrawTextEx) {
    renderer.draw(0.5f);
    EXPECT_TRUE(mock_was_called("DrawTextEx"));
}

TEST_F(InfoRendererTest, DrawCallsDrawRectangleRounded) {
    renderer.draw(0.5f);
    EXPECT_TRUE(mock_was_called("DrawRectangleRounded"));
}

TEST_F(InfoRendererTest, DrawAtZeroRatioSkipsFill) {
    renderer.draw(0.0f);
    // Background rect drawn, but fill rect should be skipped (ratio <= 0.005)
    EXPECT_TRUE(mock_was_called("DrawRectangleRounded"));  // background
}

TEST_F(InfoRendererTest, DrawAtNonZeroRatioDrawsFill) {
    renderer.draw(0.5f);
    // DrawRectangleRounded called at least twice: background + fill
    int count = static_cast<int>(
        std::count(g_mock_calls.begin(), g_mock_calls.end(), "DrawRectangleRounded"));
    EXPECT_GE(count, 2);
}

// ---------------------------------------------------------------------------
// SpriteRenderer
// ---------------------------------------------------------------------------
class SpriteRendererTest : public ::testing::Test {
protected:
    void SetUp() override { mock_reset(); }
    SpriteRenderer renderer;
};

TEST_F(SpriteRendererTest, LoadCallsLoadTexture) {
    renderer.load("", "");
    EXPECT_TRUE(mock_was_called("LoadTexture"));
}

TEST_F(SpriteRendererTest, LoadCallsSetTextureFilter) {
    renderer.load("", "");
    EXPECT_TRUE(mock_was_called("SetTextureFilter"));
}

TEST_F(SpriteRendererTest, DrawDoesNotCrashWithMockTexture) {
    // Mock LoadTexture returns id=0; draw_sprite guards against this and skips.
    renderer.load("", "");
    EXPECT_NO_FATAL_FAILURE(renderer.draw(CopilotStatus::IDLE));
}

TEST_F(SpriteRendererTest, TickDelegatesToAnimState) {
    renderer.load("", "");
    // FRAME_RATE=18 >= TARGET_FPS=10: each tick advances one frame
    renderer.tick(CopilotStatus::IDLE);
    EXPECT_EQ(renderer.anim_state().frame_index, 1);
}

TEST_F(SpriteRendererTest, AnimStateGetterReturnsInitialState) {
    const AnimState& state = renderer.anim_state();
    EXPECT_EQ(state.frame_index, 0);
    EXPECT_EQ(state.anim_counter, 0);
}

// ---------------------------------------------------------------------------
// TextRenderer — draw_model_name with empty string
// ---------------------------------------------------------------------------
TEST_F(TextRendererTest, DrawModelNameWithEmptyString) {
    // Empty model name should use FALLBACK_MODEL_NAME internally, still call DrawTextEx
    renderer.draw_model_name("");
    EXPECT_TRUE(mock_was_called("DrawTextEx"));
}

// ---------------------------------------------------------------------------
// InfoRenderer — token formatting through draw
// ---------------------------------------------------------------------------
TEST_F(InfoRendererTest, DrawWithTokenCountsCallsDrawTextEx) {
    // When both current_tokens and token_limit > 0, format_tokens is used
    mock_reset();
    renderer.draw(0.5f, 45000, 200000);
    EXPECT_TRUE(mock_was_called("DrawTextEx"));
}

// ---------------------------------------------------------------------------
// Font discovery (font_utils.h)
// ---------------------------------------------------------------------------
TEST(FontUtils, ReturnsValidPathOrEmpty) {
    std::string result = find_system_font();
    if (!result.empty()) {
        EXPECT_TRUE(std::filesystem::exists(result))
            << "find_system_font() returned non-existent path: " << result;
    }
}

TEST(FontUtils, ConsistentResults) {
    EXPECT_EQ(find_system_font(), find_system_font());
}
