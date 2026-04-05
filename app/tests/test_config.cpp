#include <gtest/gtest.h>
#include "config.h"

// ---------------------------------------------------------------------------
// status_label
// ---------------------------------------------------------------------------
TEST(StatusLabel, Idle) {
    EXPECT_STREQ(status_label(CopilotStatus::IDLE), "Zzz...");
}

TEST(StatusLabel, Waiting) {
    EXPECT_STREQ(status_label(CopilotStatus::WAITING), "Waiting on you!");
}

TEST(StatusLabel, Busy) {
    EXPECT_STREQ(status_label(CopilotStatus::BUSY), "Working...");
}

// ---------------------------------------------------------------------------
// bubble_color
// ---------------------------------------------------------------------------
TEST(BubbleColor, IdleIsBlue) {
    Color c = bubble_color(CopilotStatus::IDLE);
    EXPECT_EQ(c.r, 70);
    EXPECT_EQ(c.g, 130);
    EXPECT_EQ(c.b, 220);
    EXPECT_EQ(c.a, 255);
}

TEST(BubbleColor, WaitingIsYellow) {
    Color c = bubble_color(CopilotStatus::WAITING);
    EXPECT_EQ(c.r, 230);
    EXPECT_EQ(c.g, 190);
    EXPECT_EQ(c.b, 0);
    EXPECT_EQ(c.a, 255);
}

TEST(BubbleColor, BusyIsRed) {
    Color c = bubble_color(CopilotStatus::BUSY);
    EXPECT_EQ(c.r, 220);
    EXPECT_EQ(c.g, 50);
    EXPECT_EQ(c.b, 50);
    EXPECT_EQ(c.a, 255);
}

// ---------------------------------------------------------------------------
// bubble_fill — should be a white-tinted blend of the accent color
// ---------------------------------------------------------------------------
TEST(BubbleFill, IsLighterThanAccent) {
    // For BUSY accent {220,50,50}: fill.r = 255*0.85 + 220*0.15 = ~249 > 220
    Color accent = bubble_color(CopilotStatus::BUSY);
    Color fill   = bubble_fill(CopilotStatus::BUSY);
    EXPECT_GT(fill.r, accent.r);
    EXPECT_EQ(fill.a, 255);
}

TEST(BubbleFill, BlendIsCorrect) {
    // For IDLE accent {70,130,220,255}:
    //   r = 255*0.85 + 70*0.15  = 216.75 + 10.5  = 227.25 → 227
    //   g = 255*0.85 + 130*0.15 = 216.75 + 19.5  = 236.25 → 236
    //   b = 255*0.85 + 220*0.15 = 216.75 + 33.0  = 249.75 → 249
    Color fill = bubble_fill(CopilotStatus::IDLE);
    EXPECT_EQ(fill.r, static_cast<unsigned char>(255 * 0.85f + 70  * 0.15f));
    EXPECT_EQ(fill.g, static_cast<unsigned char>(255 * 0.85f + 130 * 0.15f));
    EXPECT_EQ(fill.b, static_cast<unsigned char>(255 * 0.85f + 220 * 0.15f));
    EXPECT_EQ(fill.a, 255);
}

TEST(BubbleFill, WaitingBlend) {
    // WAITING accent {230,190,0,255}
    Color fill = bubble_fill(CopilotStatus::WAITING);
    EXPECT_EQ(fill.r, static_cast<unsigned char>(255 * 0.85f + 230 * 0.15f));
    EXPECT_EQ(fill.g, static_cast<unsigned char>(255 * 0.85f + 190 * 0.15f));
    EXPECT_EQ(fill.b, static_cast<unsigned char>(255 * 0.85f + 0   * 0.15f));
    EXPECT_EQ(fill.a, 255);
}

// ---------------------------------------------------------------------------
// model_context_limit
// ---------------------------------------------------------------------------
TEST(ModelContextLimit, ClaudeOpus1M) {
    EXPECT_EQ(model_context_limit("claude-opus-4.6-1m"), 1000000u);
}

TEST(ModelContextLimit, ClaudeSonnet) {
    EXPECT_EQ(model_context_limit("claude-sonnet-4.6"), 200000u);
    EXPECT_EQ(model_context_limit("claude-sonnet-4.5"), 200000u);
}

TEST(ModelContextLimit, ClaudeHaiku) {
    EXPECT_EQ(model_context_limit("claude-haiku-4.5"), 200000u);
}

TEST(ModelContextLimit, Gpt5) {
    EXPECT_EQ(model_context_limit("gpt-5.2"), 1000000u);
}

TEST(ModelContextLimit, Gpt4o) {
    EXPECT_EQ(model_context_limit("gpt-4o"), 128000u);
}

TEST(ModelContextLimit, UnknownModelReturnsDefault) {
    EXPECT_EQ(model_context_limit("some-unknown-model"), DEFAULT_CONTEXT_LIMIT);
    EXPECT_EQ(model_context_limit(""), DEFAULT_CONTEXT_LIMIT);
}

// ---------------------------------------------------------------------------
// model_context_limit — prefix ordering and edge cases
// ---------------------------------------------------------------------------
TEST(ModelContextLimit, ClaudeOpus46NotMistaken1M) {
    // "claude-opus-4.6" (200k) must not match the "claude-opus-4.6-1m" (1M) entry
    EXPECT_EQ(model_context_limit("claude-opus-4.6"), 200'000u);
}

TEST(ModelContextLimit, Gpt41DoesNotMatchGpt4o) {
    // "gpt-4.1" should match its own 1M entry, not the "gpt-4o" 128k entry
    EXPECT_EQ(model_context_limit("gpt-4.1"), 1'000'000u);
}

TEST(ModelContextLimit, MatchesSubstring) {
    // Model IDs in the wild may carry extra prefixes/suffixes
    EXPECT_EQ(model_context_limit("some-prefix-claude-sonnet-4.5-suffix"), 200'000u);
}

// ---------------------------------------------------------------------------
// Constexpr / layout constants
// ---------------------------------------------------------------------------
TEST(BackgroundColor, ConstexprValues) {
    EXPECT_EQ(BACKGROUND_COLOR.r, 0);
    EXPECT_EQ(BACKGROUND_COLOR.g, 0);
    EXPECT_EQ(BACKGROUND_COLOR.b, 0);
    EXPECT_EQ(BACKGROUND_COLOR.a, 50);
}

TEST(LayoutConstants, SpriteDerivedValues) {
    EXPECT_EQ(SPRITE_DISPLAY_W, SPRITE_WIDTH * SPRITE_DISPLAY_SCALE);
    EXPECT_EQ(SPRITE_DISPLAY_H, SPRITE_HEIGHT * SPRITE_DISPLAY_SCALE);
    EXPECT_EQ(TRI_TIP_Y, BUBBLE_BOTTOM + TRI_HEIGHT);
}

// ---------------------------------------------------------------------------
// DISCONNECTED status
// ---------------------------------------------------------------------------
TEST(StatusLabel, Disconnected) {
    EXPECT_STREQ(status_label(CopilotStatus::DISCONNECTED), "No session");
}

TEST(BubbleColor, DisconnectedIsGrey) {
    Color c = bubble_color(CopilotStatus::DISCONNECTED);
    EXPECT_EQ(c.r, 120);
    EXPECT_EQ(c.g, 120);
    EXPECT_EQ(c.b, 130);
    EXPECT_EQ(c.a, 255);
}

TEST(BubbleFill, DisconnectedBlend) {
    Color fill = bubble_fill(CopilotStatus::DISCONNECTED);
    EXPECT_EQ(fill.r, static_cast<unsigned char>(255 * 0.85f + 120 * 0.15f));
    EXPECT_EQ(fill.g, static_cast<unsigned char>(255 * 0.85f + 120 * 0.15f));
    EXPECT_EQ(fill.b, static_cast<unsigned char>(255 * 0.85f + 130 * 0.15f));
    EXPECT_EQ(fill.a, 255);
}

// ---------------------------------------------------------------------------
// New config constants
// ---------------------------------------------------------------------------
TEST(PollConstants, AdaptivePolling) {
    EXPECT_EQ(POLL_BUSY_MS, 200);
    EXPECT_EQ(POLL_IDLE_MS, 2000);
    EXPECT_LT(POLL_BUSY_MS, POLL_IDLE_MS);
}

TEST(PollConstants, HeartbeatTimeout) {
    EXPECT_EQ(HEARTBEAT_TIMEOUT_S, 30);
}

TEST(PollConstants, TargetFpsIs10) {
    EXPECT_EQ(TARGET_FPS, 10);
}
