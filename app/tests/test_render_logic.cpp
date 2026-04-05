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

// ---------------------------------------------------------------------------
// truncate_text — additional edge cases
// ---------------------------------------------------------------------------
TEST(TruncateText, EmptyStringUnchanged) {
    EXPECT_EQ(truncate_text("", 30), "");
}

TEST(TruncateText, MaxLenFlooredToSeven) {
    // max_len < 7 is floored to 7 internally
    std::string input = "abcdefghij";  // 10 chars
    std::string result = truncate_text(input, 3);
    EXPECT_EQ(static_cast<int>(result.size()), 7);
    EXPECT_NE(result.find("..."), std::string::npos);
}

TEST(TruncateText, EightCharsWithMaxLenSeven) {
    // 8-char input, max_len=7: truncated to exactly 7 with "..."
    std::string result = truncate_text("abcdefgh", 7);
    EXPECT_EQ(static_cast<int>(result.size()), 7);
    EXPECT_NE(result.find("..."), std::string::npos);
}

// ---------------------------------------------------------------------------
// compute_bar_color — additional coverage
// ---------------------------------------------------------------------------
TEST(ComputeBarColor, AlphaAlways230) {
    EXPECT_EQ(compute_bar_color(0.0f).a, 230);
    EXPECT_EQ(compute_bar_color(0.25f).a, 230);
    EXPECT_EQ(compute_bar_color(0.5f).a, 230);
    EXPECT_EQ(compute_bar_color(0.75f).a, 230);
    EXPECT_EQ(compute_bar_color(1.0f).a, 230);
}

TEST(ComputeBarColor, QuarterRatioIsGreenYellow) {
    // At 0.25 we're in the green→yellow transition: red increases, green stays 200
    Color c = compute_bar_color(0.25f);
    EXPECT_GT(c.r, 80);    // above green's base red
    EXPECT_LT(c.r, 255);   // below full red
    EXPECT_EQ(c.g, 200);   // green channel constant in first half
}

// ---------------------------------------------------------------------------
// compute_bar_fill_width — additional coverage
// ---------------------------------------------------------------------------
TEST(ComputeBarFillWidth, SmallRatioStillMeetsMinimum) {
    // Very small ratio on a wide bar: result >= 2*BAR_RADIUS
    float result = compute_bar_fill_width(0.001f, 1000.0f);
    EXPECT_GE(result, 2.0f * BAR_RADIUS);
}

// ---------------------------------------------------------------------------
// sanitize_display_string
// ---------------------------------------------------------------------------
TEST(SanitizeDisplayString, PassesThroughCleanText) {
    EXPECT_EQ(sanitize_display_string("Hello World"), "Hello World");
}

TEST(SanitizeDisplayString, StripsControlChars) {
    EXPECT_EQ(sanitize_display_string("Hello\x01\x02World"), "HelloWorld");
}

TEST(SanitizeDisplayString, StripsNewlines) {
    EXPECT_EQ(sanitize_display_string("Line1\nLine2\rLine3"), "Line1Line2Line3");
}

TEST(SanitizeDisplayString, ConvertsTabsToSpaces) {
    EXPECT_EQ(sanitize_display_string("Hello\tWorld"), "Hello World");
}

TEST(SanitizeDisplayString, StripsNullBytes) {
    std::string input = std::string("Hello") + '\0' + "World";
    EXPECT_EQ(sanitize_display_string(input), "HelloWorld");
}

TEST(SanitizeDisplayString, EnforcesMaxLength) {
    std::string long_str(500, 'A');
    std::string result = sanitize_display_string(long_str, 10);
    EXPECT_EQ(result.size(), 10u);
}

TEST(SanitizeDisplayString, DefaultMaxLenIsMAX_TEXT_LEN) {
    std::string long_str(500, 'A');
    std::string result = sanitize_display_string(long_str);
    EXPECT_EQ(result.size(), static_cast<size_t>(MAX_TEXT_LEN));
}

TEST(SanitizeDisplayString, EmptyStringStaysEmpty) {
    EXPECT_EQ(sanitize_display_string(""), "");
}

TEST(SanitizeDisplayString, PreservesUTF8Continuation) {
    // High bytes (>= 0x80) are kept for UTF-8 multi-byte sequences
    std::string utf8 = "Hello \xC3\xA9";  // "Hello é"
    EXPECT_EQ(sanitize_display_string(utf8), utf8);
}

TEST(SanitizeDisplayString, StripsDEL) {
    EXPECT_EQ(sanitize_display_string("Hello\x7FWorld"), "HelloWorld");
}

TEST(SanitizeDisplayString, AllControlCharsReturnEmpty) {
    std::string input = "\x01\x02\x03\x04\x05\x06\x07\x08\x0B\x0C\x0E\x0F";
    EXPECT_EQ(sanitize_display_string(input), "");
}

// ---------------------------------------------------------------------------
// compute_bar_fill_width — clamp above 1.0
// ---------------------------------------------------------------------------
TEST(ComputeBarFillWidth, ClampsAboveOne) {
    float bar_width = static_cast<float>(BAR_WIDTH);
    float result = compute_bar_fill_width(1.5f, bar_width);
    EXPECT_FLOAT_EQ(result, bar_width);  // clamped to 1.0 → full width
}
