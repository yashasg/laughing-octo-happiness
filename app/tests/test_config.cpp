#include <gtest/gtest.h>
#include "config.h"

// ---------------------------------------------------------------------------
// status_label
// ---------------------------------------------------------------------------
TEST(StatusLabel, Idle) {
    EXPECT_STREQ(status_label(CopilotStatus::IDLE), "Zzz...");
}

TEST(StatusLabel, Waiting) {
    EXPECT_STREQ(status_label(CopilotStatus::WAITING), "Your turn!");
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

TEST(BubbleColor, WaitingIsGreen) {
    Color c = bubble_color(CopilotStatus::WAITING);
    EXPECT_EQ(c.r, 0);
    EXPECT_EQ(c.g, 200);
    EXPECT_EQ(c.b, 80);
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
