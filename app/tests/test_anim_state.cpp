#include <gtest/gtest.h>
#include "anim_state.h"

// ---------------------------------------------------------------------------
// Helper: advance exactly N animation frames by calling tick() enough times.
// FRAME_RATE=18, TARGET_FPS=30 → first advance fires on tick 2 (18+18=36 >= 30).
// ---------------------------------------------------------------------------
static void advance_frames(AnimState& s, CopilotStatus status, int frames) {
    // Each frame advance requires ceil(TARGET_FPS / FRAME_RATE) ticks.
    // With FRAME_RATE=18, TARGET_FPS=30: 2 ticks per frame advance.
    for (int i = 0; i < frames; ++i) {
        // Keep ticking until frame_index advances once.
        int before = s.frame_index;
        for (int t = 0; t < 1000; ++t) {
            s.tick(status);
            if (s.frame_index != before) break;
        }
    }
}

// ---------------------------------------------------------------------------
// Basic initial state
// ---------------------------------------------------------------------------
TEST(AnimState, StartsAtFrameZero) {
    AnimState s;
    EXPECT_EQ(s.frame_index, 0);
    EXPECT_EQ(s.anim_counter, 0);
    EXPECT_EQ(s.idle_frame_count, IDLE_FRAMES);
    EXPECT_EQ(s.run_frame_count, RUN_FRAMES);
}

// ---------------------------------------------------------------------------
// No advance before threshold
// ---------------------------------------------------------------------------
TEST(AnimState, NoAdvanceBeforeThreshold) {
    AnimState s;
    // 1 tick: anim_counter = 18, which is < 30 → no frame advance
    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 0);
    EXPECT_EQ(s.anim_counter, 18);
}

// ---------------------------------------------------------------------------
// Advances frame after enough ticks
// ---------------------------------------------------------------------------
TEST(AnimState, AdvancesFrameAfterEnoughTicks) {
    AnimState s;
    // tick 1: counter=18 (no advance)
    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 0);
    // tick 2: counter=36 >= 30 → advance; counter becomes 36-30=6
    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 1);
    EXPECT_EQ(s.anim_counter, 6);
}

// ---------------------------------------------------------------------------
// Wraps at idle frame count
// ---------------------------------------------------------------------------
TEST(AnimState, WrapsAtIdleFrameCount) {
    AnimState s;
    s.idle_frame_count = 3;

    // Advance 3 frames → should wrap back to 0
    advance_frames(s, CopilotStatus::IDLE, 3);
    EXPECT_EQ(s.frame_index, 0);
}

// ---------------------------------------------------------------------------
// Uses busy frame count when BUSY
// ---------------------------------------------------------------------------
TEST(AnimState, UsesBusyFrameCountWhenBusy) {
    AnimState s;
    s.run_frame_count = 4;

    // Advance 4 frames with BUSY status → should wrap back to 0
    advance_frames(s, CopilotStatus::BUSY, 4);
    EXPECT_EQ(s.frame_index, 0);
}

// ---------------------------------------------------------------------------
// Uses idle frame count when IDLE
// ---------------------------------------------------------------------------
TEST(AnimState, UsesIdleFrameCountWhenIdle) {
    AnimState s;
    s.idle_frame_count = 5;

    // Advance 5 frames with IDLE status → should wrap back to 0
    advance_frames(s, CopilotStatus::IDLE, 5);
    EXPECT_EQ(s.frame_index, 0);
}

// ---------------------------------------------------------------------------
// Multiple ticks in same frame when FRAME_RATE < TARGET_FPS
// ---------------------------------------------------------------------------
TEST(AnimState, WaitsForThreshold) {
    // FRAME_RATE=18, TARGET_FPS=30: need 2 ticks to cross threshold.
    AnimState s;

    // After 1 tick: still frame 0
    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 0);

    // After 2 ticks: frame advances to 1
    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 1);

    // After 3 ticks: counter = 6+18=24, still < 30 → frame stays 1
    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 1);

    // After 4 ticks: counter = 24+18=42 >= 30 → frame advances to 2
    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 2);
}
