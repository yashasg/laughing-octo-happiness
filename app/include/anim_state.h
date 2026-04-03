#pragma once
#include "config.h"

/// Pure animation state: no raylib calls, fully testable.
struct AnimState {
    int frame_index      = 0;
    int anim_counter     = 0;
    int idle_frame_count = IDLE_FRAMES;
    int run_frame_count  = RUN_FRAMES;

    /// Advance one game-loop tick. Pure arithmetic, no GPU involvement.
    void tick(CopilotStatus status);
};
