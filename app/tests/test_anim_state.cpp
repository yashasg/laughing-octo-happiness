#include <gtest/gtest.h>
#include "anim_state.h"

// ---------------------------------------------------------------------------
// Helper: advance exactly N animation frames by calling tick() enough times.
// FRAME_RATE=18, TARGET_FPS=10 → frame advances every tick (18 >= 10).
// ---------------------------------------------------------------------------
static void advance_frames(AnimState& s, CopilotStatus status, int frames) {
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
// Frame advances on first tick (FRAME_RATE=18 >= TARGET_FPS=10)
// ---------------------------------------------------------------------------
TEST(AnimState, AdvancesOnFirstTick) {
    AnimState s;
    // 1 tick: counter = 18, 18 >= 10 → advance, counter = 8
    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 1);
    EXPECT_EQ(s.anim_counter, FRAME_RATE - TARGET_FPS);  // 8
}

// ---------------------------------------------------------------------------
// Advances frame on every tick when FRAME_RATE >= TARGET_FPS
// ---------------------------------------------------------------------------
TEST(AnimState, AdvancesEveryTick) {
    AnimState s;
    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 1);
    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 2);
    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 3);
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
// Counter accumulates remainder correctly across ticks
// ---------------------------------------------------------------------------
TEST(AnimState, CounterAccumulatesRemainder) {
    // FRAME_RATE=18, TARGET_FPS=10 → every tick advances a frame.
    // Tick 1: counter=18, advance, counter=8
    // Tick 2: counter=26, advance, counter=16
    // Tick 3: counter=34, advance, counter=24
    AnimState s;

    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 1);
    EXPECT_EQ(s.anim_counter, FRAME_RATE - TARGET_FPS);  // 8

    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 2);

    s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 3);
}

// ---------------------------------------------------------------------------
// Zero frame count — must not crash (division by zero guard)
// ---------------------------------------------------------------------------
TEST(AnimState, ZeroFrameCountDoesNotCrash) {
    AnimState s;
    s.idle_frame_count = 0;
    s.run_frame_count = 0;
    for (int i = 0; i < 100; ++i) s.tick(CopilotStatus::IDLE);
    EXPECT_EQ(s.frame_index, 0);
    for (int i = 0; i < 100; ++i) s.tick(CopilotStatus::BUSY);
    EXPECT_EQ(s.frame_index, 0);
}

// ---------------------------------------------------------------------------
// WAITING status uses idle frame count (same code path as IDLE)
// ---------------------------------------------------------------------------
TEST(AnimState, WaitingUsesIdleFrameCount) {
    AnimState s;
    s.idle_frame_count = 3;
    s.run_frame_count = 5;
    advance_frames(s, CopilotStatus::WAITING, 3);
    EXPECT_EQ(s.frame_index, 0);  // wrapped around 3
}

// ---------------------------------------------------------------------------
// Status switch mid-animation — frame_index carries over
// ---------------------------------------------------------------------------
TEST(AnimState, StatusSwitchMidAnimation) {
    AnimState s;
    s.idle_frame_count = 10;
    s.run_frame_count = 16;
    advance_frames(s, CopilotStatus::IDLE, 3);
    EXPECT_EQ(s.frame_index, 3);
    // Switch to BUSY — frame_index continues, wraps at run_frame_count
    advance_frames(s, CopilotStatus::BUSY, 1);
    EXPECT_EQ(s.frame_index, 4);
}

// ---------------------------------------------------------------------------
// Single frame count — always stays at 0
// ---------------------------------------------------------------------------
TEST(AnimState, SingleFrameCountWrapsImmediately) {
    AnimState s;
    s.idle_frame_count = 1;
    advance_frames(s, CopilotStatus::IDLE, 5);
    EXPECT_EQ(s.frame_index, 0);  // (0+1)%1 = 0
}

// ---------------------------------------------------------------------------
// DISCONNECTED uses idle frame count (same as IDLE/WAITING)
// ---------------------------------------------------------------------------
TEST(AnimState, DisconnectedUsesIdleFrameCount) {
    AnimState s;
    s.idle_frame_count = 4;
    s.run_frame_count = 8;
    advance_frames(s, CopilotStatus::DISCONNECTED, 4);
    EXPECT_EQ(s.frame_index, 0);  // wrapped around 4
}
