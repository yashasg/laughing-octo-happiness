#pragma once

#include "anim_state.h"
#include "config.h"
#include "raylib.h"
#include <string>

/// Renders the character sprite animation.
/// Loads spritesheets once; all draw calls use GPU textures.
class SpriteRenderer {
public:
    SpriteRenderer();
    ~SpriteRenderer();
    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    /// Load sprite sheets. Call after InitWindow().
    void load(const std::string& idle_path, const std::string& run_path);

    /// Advance animation frame counter (call once per game-loop tick).
    void tick(CopilotStatus status);

    /// Draw the current animation frame. Call between BeginDrawing()/EndDrawing().
    void draw(CopilotStatus status) const;

    /// Read-only access to animation state (useful for testing).
    const AnimState& anim_state() const { return m_anim; }

private:
    void draw_sprite(CopilotStatus status) const;

    Texture2D m_idle_sheet{};
    Texture2D m_run_sheet{};
    AnimState m_anim{};
};
