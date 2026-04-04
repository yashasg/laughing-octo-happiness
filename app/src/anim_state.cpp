#include "anim_state.h"

void AnimState::tick(CopilotStatus status) {
    anim_counter += FRAME_RATE;
    if (anim_counter >= TARGET_FPS) {
        anim_counter -= TARGET_FPS;
        const int count = (status == CopilotStatus::BUSY) ? run_frame_count : idle_frame_count;
        if (count > 0) frame_index = (frame_index + 1) % count;
    }
}
