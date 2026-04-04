#include <gtest/gtest.h>
#include "render_logic.h"

// ---------------------------------------------------------------------------
// truncate_text
// ---------------------------------------------------------------------------

TEST(TruncateText, ShortTextUnchanged) {
    std::string input = "Hello";
    EXPECT_EQ(truncate_text(input, 25), "Hello");
}

TEST(TruncateText, ExactLengthUnchanged) {
    // Exactly MAX_TEXT_LEN characters — should pass through unchanged.
    std::string input(MAX_TEXT_LEN, 'x');
    EXPECT_EQ(truncate_text(input, MAX_TEXT_LEN), input);
}

TEST(TruncateText, LongTextTruncated) {
    // One character over MAX_TEXT_LEN — should be center-ellipsized with "..."
    std::string input(MAX_TEXT_LEN + 1, 'A');
    std::string result = truncate_text(input, MAX_TEXT_LEN);
    int keep = MAX_TEXT_LEN - 3;
    int head = (keep + 1) / 2;
    int tail = keep - head;
    EXPECT_EQ(result.substr(0, head), std::string(head, 'A'));
    EXPECT_EQ(result.substr(head, 3), "...");
    EXPECT_EQ(result.substr(head + 3), std::string(tail, 'A'));
}

TEST(TruncateText, TruncatedLengthIsMaxLen) {
    // Result length must be exactly max_len when input is over max_len.
    std::string input(MAX_TEXT_LEN + 10, 'B');
    std::string result = truncate_text(input, MAX_TEXT_LEN);
    EXPECT_EQ(static_cast<int>(result.size()), MAX_TEXT_LEN);
}

TEST(TruncateText, CenterEllipsisPreservesEnds) {
    std::string input = "Let me start by reading the files in parallel";
    std::string result = truncate_text(input, 30);
    // Should have "..." in the middle, start matches, end matches
    EXPECT_EQ(static_cast<int>(result.size()), 30);
    auto pos = result.find("...");
    ASSERT_NE(pos, std::string::npos);
    // Head should be the start of the original string
    EXPECT_EQ(result.substr(0, pos), input.substr(0, pos));
    // Tail should be the end of the original string
    std::string tail = result.substr(pos + 3);
    EXPECT_EQ(tail, input.substr(input.size() - tail.size()));
}

TEST(TruncateText, MaxTextLenIs30) {
    EXPECT_EQ(MAX_TEXT_LEN, 30);
}

// ---------------------------------------------------------------------------
// compute_bar_color
// ---------------------------------------------------------------------------

TEST(ComputeBarColor, ZeroRatioIsGreen) {
    Color c = compute_bar_color(0.0f);
    EXPECT_LT(c.r, 100);
    EXPECT_EQ(c.g, 200);
    EXPECT_EQ(c.b, 60);
    EXPECT_EQ(c.a, 230);
}

TEST(ComputeBarColor, HalfRatioIsYellow) {
    Color c = compute_bar_color(0.5f);
    EXPECT_EQ(c.r, 255);
    EXPECT_EQ(c.g, 200);
    EXPECT_EQ(c.b, 60);
}

TEST(ComputeBarColor, FullRatioIsRed) {
    Color c = compute_bar_color(1.0f);
    EXPECT_EQ(c.r, 255);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 60);
}

TEST(ComputeBarColor, ClampsBelowZero) {
    // ratio < 0 should clamp to 0 — same result as compute_bar_color(0.0f)
    Color clamped = compute_bar_color(-0.5f);
    Color zero    = compute_bar_color(0.0f);
    EXPECT_EQ(clamped.r, zero.r);
    EXPECT_EQ(clamped.g, zero.g);
    EXPECT_EQ(clamped.b, zero.b);
    EXPECT_EQ(clamped.a, zero.a);
}

TEST(ComputeBarColor, ClampsAboveOne) {
    // ratio > 1 should clamp to 1 — same result as compute_bar_color(1.0f)
    Color clamped = compute_bar_color(1.5f);
    Color full    = compute_bar_color(1.0f);
    EXPECT_EQ(clamped.r, full.r);
    EXPECT_EQ(clamped.g, full.g);
    EXPECT_EQ(clamped.b, full.b);
    EXPECT_EQ(clamped.a, full.a);
}

// ---------------------------------------------------------------------------
// compute_bar_fill_width
// ---------------------------------------------------------------------------

TEST(ComputeBarFillWidth, ZeroRatioReturnsMinimum) {
    float result = compute_bar_fill_width(0.0f, static_cast<float>(BAR_WIDTH));
    EXPECT_FLOAT_EQ(result, 2.0f * BAR_RADIUS);
}

TEST(ComputeBarFillWidth, FullRatioReturnsBarWidth) {
    float bar_width = static_cast<float>(BAR_WIDTH);
    float result = compute_bar_fill_width(1.0f, bar_width);
    EXPECT_FLOAT_EQ(result, bar_width);
}

TEST(ComputeBarFillWidth, HalfRatioReturnsHalf) {
    float bar_width = static_cast<float>(BAR_WIDTH);
    float result = compute_bar_fill_width(0.5f, bar_width);
    EXPECT_FLOAT_EQ(result, bar_width * 0.5f);
}

TEST(ComputeBarFillWidth, ClampsBelowZero) {
    // Negative ratio should clamp to 0 → minimum fill (2 * BAR_RADIUS)
    float result = compute_bar_fill_width(-1.0f, static_cast<float>(BAR_WIDTH));
    EXPECT_FLOAT_EQ(result, 2.0f * BAR_RADIUS);
}
