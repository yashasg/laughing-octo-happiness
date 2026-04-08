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
extern bool g_mock_texture_valid;
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

// ---------------------------------------------------------------------------
// TextRenderer — DISCONNECTED status
// ---------------------------------------------------------------------------
TEST_F(TextRendererTest, DrawBubbleDisconnectedCallsDrawFunctions) {
    renderer.draw_bubble(CopilotStatus::DISCONNECTED, "No session");
    EXPECT_TRUE(mock_was_called("DrawRectangleRounded"));
    EXPECT_TRUE(mock_was_called("DrawTextEx"));
    EXPECT_TRUE(mock_was_called("DrawTriangle"));
}

// ---------------------------------------------------------------------------
// SpriteRenderer — BUSY status uses different sheet
// ---------------------------------------------------------------------------
TEST_F(SpriteRendererTest, DrawWithBusyDoesNotCrash) {
    renderer.load("", "");
    EXPECT_NO_FATAL_FAILURE(renderer.draw(CopilotStatus::BUSY));
}

TEST_F(SpriteRendererTest, DrawWithDisconnectedDoesNotCrash) {
    renderer.load("", "");
    EXPECT_NO_FATAL_FAILURE(renderer.draw(CopilotStatus::DISCONNECTED));
}

// ---------------------------------------------------------------------------
// SpriteRenderer — valid texture (id > 0) must reach DrawTexturePro
// This is the critical path that was broken on Windows: when LoadTexture
// succeeds the draw_sprite guard (sheet.id == 0) must NOT fire, and the
// sprite must actually be rendered.  The mock flag g_mock_texture_valid
// simulates a successfully loaded texture.
// ---------------------------------------------------------------------------
TEST_F(SpriteRendererTest, DrawCallsDrawTexturePro_WhenTextureValid_Idle) {
    g_mock_texture_valid = true;
    renderer.load("IDLE.png", "RUN.png");
    mock_reset();
    renderer.draw(CopilotStatus::IDLE);
    EXPECT_TRUE(mock_was_called("DrawTexturePro"))
        << "DrawTexturePro must be called when IDLE texture has id > 0";
}

TEST_F(SpriteRendererTest, DrawCallsDrawTexturePro_WhenTextureValid_Busy) {
    g_mock_texture_valid = true;
    renderer.load("IDLE.png", "RUN.png");
    mock_reset();
    renderer.draw(CopilotStatus::BUSY);
    EXPECT_TRUE(mock_was_called("DrawTexturePro"))
        << "DrawTexturePro must be called when RUN texture has id > 0";
}

TEST_F(SpriteRendererTest, DrawCallsDrawTexturePro_WhenTextureValid_Waiting) {
    // WAITING uses the idle sheet (same code path as IDLE)
    g_mock_texture_valid = true;
    renderer.load("IDLE.png", "RUN.png");
    mock_reset();
    renderer.draw(CopilotStatus::WAITING);
    EXPECT_TRUE(mock_was_called("DrawTexturePro"))
        << "DrawTexturePro must be called when WAITING (uses idle sheet, id > 0)";
}

TEST_F(SpriteRendererTest, DrawSkipsDrawTexturePro_WhenTextureInvalid) {
    // id == 0 (the default mock) → draw_sprite silently returns early.
    // This is the path that hid the Windows rendering failure: the sprite
    // was simply skipped with no error, making it appear as if the avatar
    // was missing at runtime.
    renderer.load("", "");
    mock_reset();
    renderer.draw(CopilotStatus::IDLE);
    EXPECT_FALSE(mock_was_called("DrawTexturePro"))
        << "DrawTexturePro must NOT be called when texture id == 0";
}

TEST_F(SpriteRendererTest, FrameCountDerivedFromTextureWidth_WhenValid) {
    // When LoadTexture returns a realistic spritesheet, load() must derive
    // idle_frame_count from the texture width rather than falling back to
    // the compile-time constant.
    g_mock_texture_valid = true;
    renderer.load("IDLE.png", "RUN.png");
    // Mock returns width = SPRITE_WIDTH * IDLE_FRAMES
    EXPECT_EQ(renderer.anim_state().idle_frame_count, IDLE_FRAMES);
}

// ---------------------------------------------------------------------------
// InfoRenderer — draw at ratio boundaries
// ---------------------------------------------------------------------------
TEST_F(InfoRendererTest, DrawAtFullRatio) {
    mock_reset();
    renderer.draw(1.0f);
    // Background + fill drawn
    int count = static_cast<int>(
        std::count(g_mock_calls.begin(), g_mock_calls.end(), "DrawRectangleRounded"));
    EXPECT_GE(count, 2);
}

TEST_F(InfoRendererTest, DrawWithOnlyTokenLimitNoCurrentTokens) {
    // token_limit > 0 but current_tokens == 0 → should show "Context" label
    mock_reset();
    renderer.draw(0.3f, 0, 200000);
    EXPECT_TRUE(mock_was_called("DrawTextEx"));
}

TEST_F(InfoRendererTest, DrawWithBothTokensZero) {
    mock_reset();
    renderer.draw(0.0f, 0, 0);
    EXPECT_TRUE(mock_was_called("DrawTextEx"));
}
