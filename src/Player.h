// ============================================================
//  Player.h  -  啾比：移动、二段跳、土狼时间与洋葱式手感
// ============================================================
#pragma once

#include "Common.h"

namespace jubi {

class Renderer;
class Level;
class Audio;

struct Player {
    RectF box;
    Vec2 vel;
    bool onGround = false;
    int jumps = 0;
    float coyote = 0.0f;
    float buffer = 0.0f;
    float jumpCut = 0.0f;
    float bounceCooldown = 0.0f;
    float animTime = 0.0f;
    float runPhase = 0.0f;
    float squash = 0.0f;
    float stretch = 0.0f;
    float landTimer = 0.0f;
    bool facingRight = true;
    bool jumpHeldLast = false;
    int standingMover = -1;
    int footstepTick = 0;

    void reset(Vec2 topLeft);
    void update(float dt, const InputState& input, Level& level, Audio& audio);
    void draw(Renderer& r) const;

    RectF hitbox() const { return box; }
    Vec2 center() const { return box.center(); }
};

// 供标题页 / 结算页复用的啾比绘制
void drawJubi(Renderer& r, float cx, float cy, float scale, bool facingRight,
              float animTime, float runPhase, bool airborne, float squash,
              float stretch);

} // namespace jubi
