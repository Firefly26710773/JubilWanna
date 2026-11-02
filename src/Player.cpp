// ============================================================
//  Player.cpp  -  移动手感与啾比的程序化绘制
// ============================================================
#include "Player.h"

#include "Audio.h"
#include "Level.h"
#include "Renderer.h"

#include <cmath>

namespace jubi {

namespace {

constexpr float kRunSpeed = 300.0f;
constexpr float kGroundAccel = 3000.0f;
constexpr float kAirAccel = 1900.0f;
constexpr float kJumpSpeed = 760.0f;
constexpr float kDoubleJumpSpeed = 700.0f;
constexpr float kGravityUp = 2200.0f;
constexpr float kGravityDown = 2500.0f;
constexpr float kMaxFall = 1100.0f;
constexpr float kBounceSpeed = 1250.0f;

} // namespace

void Player::reset(Vec2 topLeft) {
    box = RectF(topLeft.x, topLeft.y, kPlayerW, kPlayerH);
    vel = Vec2{};
    onGround = false;
    jumps = 1;
    coyote = 0.0f;
    buffer = 0.0f;
    jumpCut = 0.0f;
    bounceCooldown = 0.0f;
    animTime = 0.0f;
    runPhase = 0.0f;
    squash = 0.0f;
    stretch = 0.0f;
    landTimer = 0.0f;
    facingRight = true;
    jumpHeldLast = false;
    standingMover = -1;
}

void Player::update(float dt, const InputState& input, Level& level, Audio& audio) {
    animTime += dt;
    if (squash > 0.0f) squash = approach(squash, 0.0f, dt * 5.0f);
    if (stretch > 0.0f) stretch = approach(stretch, 0.0f, dt * 5.5f);
    if (landTimer > 0.0f) landTimer -= dt;
    if (bounceCooldown > 0.0f) bounceCooldown -= dt;
    if (buffer > 0.0f) buffer -= dt;
    if (jumpCut > 0.0f) jumpCut -= dt;

    // 被移动平台带着走
    if (standingMover >= 0) {
        Vec2 d = level.moverDelta(standingMover);
        box.x += d.x;
        box.y += d.y;
        if (box.x < 0.0f) box.x = 0.0f;
        if (box.x + box.w > kScreenW) box.x = kScreenW - box.w;
    }

    bool wasGround = onGround;

    float target = 0.0f;
    if (input.left) target -= kRunSpeed;
    if (input.right) target += kRunSpeed;
    if (input.left && !input.right) facingRight = false;
    if (input.right && !input.left) facingRight = true;

    float accel = onGround ? kGroundAccel : kAirAccel;
    if (std::fabs(target) < 0.01f) accel = onGround ? kGroundAccel * 1.15f : kAirAccel * 0.75f;
    vel.x = approach(vel.x, target, accel * dt);

    if (input.jumpPressed) buffer = 0.12f;
    if (onGround) coyote = 0.1f;
    else coyote = (std::max)(0.0f, coyote - dt);

    if (!onGround && coyote <= 0.0f && jumps == 0) jumps = 1;

    if (buffer > 0.0f) {
        if (onGround || coyote > 0.0f) {
            vel.y = -kJumpSpeed;
            onGround = false;
            jumps = 1;
            coyote = 0.0f;
            buffer = 0.0f;
            jumpCut = 0.13f;
            stretch = 1.0f;
            audio.play(Sfx::Jump);
        } else if (jumps < 2) {
            vel.y = -kDoubleJumpSpeed;
            jumps = 2;
            buffer = 0.0f;
            jumpCut = 0.13f;
            stretch = 1.0f;
            audio.play(Sfx::DoubleJump);
        }
    }

    // 松开跳跃键截断上升速度：短按小跳，长按大跳
    if (!input.jumpHeld && jumpCut > 0.0f && vel.y < 0.0f) {
        vel.y *= 0.45f;
        jumpCut = 0.0f;
    }

    float gravity = vel.y < 0.0f ? kGravityUp : kGravityDown;
    vel.y += gravity * dt;
    if (vel.y > kMaxFall) vel.y = kMaxFall;

    if (level.moveX(box, vel.x * dt)) {
        vel.x = 0.0f;
    }

    standingMover = -1;
    float fallSpeed = vel.y;
    MoveYResult hit = level.moveY(box, vel.y * dt);
    onGround = false;
    if (hit.landed) {
        if (vel.y < 0.0f) {
            vel.y = 0.0f;
        } else {
            vel.y = 0.0f;
            onGround = true;
            jumps = 0;
            jumpCut = 0.0f;
            if (!wasGround && fallSpeed > 340.0f) {
                audio.play(Sfx::Land);
                squash = 1.0f;
                landTimer = 0.16f;
            }
            if (hit.onMover) standingMover = hit.moverIndex;
        }
    }

    if (hit.bounce && bounceCooldown <= 0.0f) {
        vel.y = -kBounceSpeed;
        onGround = false;
        jumps = 0;
        bounceCooldown = 0.35f;
        squash = 0.0f;
        stretch = 1.0f;
        audio.play(Sfx::Bounce);
    } else if (onGround && bounceCooldown <= 0.0f && level.touchingBounce(box)) {
        vel.y = -kBounceSpeed;
        onGround = false;
        jumps = 0;
        bounceCooldown = 0.35f;
        squash = 0.0f;
        stretch = 1.0f;
        audio.play(Sfx::Bounce);
    }

    if (onGround) {
        runPhase += std::fabs(vel.x) * dt * 0.055f;
    } else {
        runPhase += dt * 4.0f;
    }
}

void Player::draw(Renderer& r) const {
    if (onGround) {
        float squashX = 1.0f + squash * 0.22f;
        r.ellipse(RectF(box.center().x - 18 * squashX, box.y + box.h - 5,
                        36 * squashX, 7), rgb(20, 18, 30));
    }
    drawJubi(r, box.center().x, box.center().y - 1.0f, 1.0f, facingRight,
             animTime, runPhase, !onGround, squash, stretch);
}

// ============================================================
//  啾比绘制
// ============================================================
void drawJubi(Renderer& r, float cx, float cy, float scale, bool facingRight,
              float animTime, float runPhase, bool airborne, float squash,
              float stretch) {
    float f = facingRight ? 1.0f : -1.0f;
    float sx = (1.0f + squash * 0.20f - stretch * 0.05f) * scale;
    float sy = (1.0f - squash * 0.17f + stretch * 0.10f) * scale;

    float bob = airborne ? 0.0f : std::sin(animTime * 7.0f) * 1.2f;
    cy += bob;

    Color outline = rgb(92, 54, 26);
    Color body = rgb(255, 214, 76);
    Color belly = rgb(255, 238, 154);
    Color wing = rgb(255, 242, 178);
    Color beak = rgb(255, 138, 46);
    Color cape = rgb(66, 112, 210);
    Color capeDark = rgb(38, 72, 158);

    // 斗篷（在身体后面）
    float wave = std::sin(animTime * 7.2f) * 4.0f + (airborne ? 5.0f : 0.0f);
    Vec2 capePts[5] = {
        Vec2(cx - f * 8 * sx, cy - 12 * sy),
        Vec2(cx - f * 25 * sx, cy - 8 * sy + wave * 0.4f),
        Vec2(cx - f * 32 * sx, cy + 4 * sy + wave),
        Vec2(cx - f * 25 * sx, cy + 16 * sy + wave * 0.6f),
        Vec2(cx - f * 8 * sx, cy + 13 * sy)
    };
    r.polygon(capePts, 5, capeDark);
    Vec2 capeInner[4] = {
        Vec2(cx - f * 9 * sx, cy - 8 * sy),
        Vec2(cx - f * 22 * sx, cy - 4 * sy + wave * 0.3f),
        Vec2(cx - f * 27 * sx, cy + 6 * sy + wave * 0.7f),
        Vec2(cx - f * 9 * sx, cy + 9 * sy)
    };
    r.polygon(capeInner, 4, cape);
    r.circle(cx - f * 9 * sx, cy - 10 * sy, 3.2f * scale, rgb(246, 210, 110));

    // 尾巴
    Vec2 tail[3] = {
        Vec2(cx - f * 14 * sx, cy + 6 * sy),
        Vec2(cx - f * 27 * sx, cy + 2 * sy),
        Vec2(cx - f * 16 * sx, cy + 12 * sy)
    };
    r.polygon(tail, 3, darken(body, 0.08f));
    r.line(Vec2(cx - f * 15 * sx, cy + 7 * sy), Vec2(cx - f * 25 * sx, cy + 4 * sy), outline, 1);

    // 小脚
    float step = airborne ? -2.0f : std::sin(runPhase) * 3.0f;
    float footY = cy + 15 * sy;
    if (airborne) {
        r.line(Vec2(cx - 4 * sx, footY), Vec2(cx - 8 * sx, footY + 5 * sy), beak, 3);
        r.line(Vec2(cx + 4 * sx, footY), Vec2(cx + 8 * sx, footY + 5 * sy), beak, 3);
    } else {
        r.line(Vec2(cx - 6 * sx, footY), Vec2(cx - 7 * sx + step, footY + 6 * sy), beak, 3);
        r.line(Vec2(cx + 6 * sx, footY), Vec2(cx + 7 * sx - step, footY + 6 * sy), beak, 3);
        r.line(Vec2(cx - 7 * sx + step, footY + 6 * sy),
               Vec2(cx - 1 * sx + step, footY + 6 * sy), beak, 2);
        r.line(Vec2(cx + 7 * sx - step, footY + 6 * sy),
               Vec2(cx + 1 * sx - step, footY + 6 * sy), beak, 2);
    }

    // 身体轮廓 + 主体
    r.ellipse(RectF(cx - 19 * sx, cy - 17 * sy, 38 * sx, 35 * sy), outline);
    r.ellipse(RectF(cx - 17 * sx, cy - 16 * sy, 34 * sx, 32 * sy), body);
    r.ellipse(RectF(cx - 13 * sx, cy + 1 * sy, 26 * sx, 16 * sy), belly);

    // 头顶呆毛
    r.line(Vec2(cx - 2 * sx, cy - 16 * sy), Vec2(cx - 7 * sx, cy - 24 * sy), outline, 2);
    r.circle(cx - 8 * sx, cy - 25 * sy, 2.1f * scale, body);
    r.line(Vec2(cx + 3 * sx, cy - 16 * sy), Vec2(cx + 7 * sx, cy - 23 * sy), outline, 2);
    r.circle(cx + 8 * sx, cy - 24 * sy, 1.8f * scale, body);

    // 翅膀
    float flap = airborne ? std::sin(animTime * 16.0f) * 5.0f : std::sin(animTime * 4.0f) * 1.0f;
    r.ellipse(RectF(cx - f * 17 * sx - 6 * sx, cy - 6 * sy - flap,
                    13 * sx, 16 * sy), darken(body, 0.04f));
    r.ellipse(RectF(cx - f * 17 * sx - 4 * sx, cy - 5 * sy - flap,
                    9 * sx, 11 * sy), wing);

    // 眼睛
    float eyeX = cx + f * 5.0f * sx;
    float eyeY = cy - 5.0f * sy;
    r.circle(eyeX, eyeY, 5.2f * scale, rgb(255, 255, 255));
    r.circle(eyeX + f * 1.8f * scale, eyeY + 0.2f * scale, 2.7f * scale, rgb(38, 34, 44));
    r.circle(eyeX + f * 2.6f * scale, eyeY - 1.0f * scale, 1.0f * scale, rgb(255, 255, 255));

    float eye2X = cx + f * 12.5f * sx;
    r.circle(eye2X, eyeY - 0.8f * sy, 3.9f * scale, rgb(255, 255, 255));
    r.circle(eye2X + f * 1.4f * scale, eyeY - 0.7f * sy, 2.0f * scale, rgb(38, 34, 44));
    r.circle(eye2X + f * 2.0f * scale, eyeY - 1.5f * sy, 0.8f * scale, rgb(255, 255, 255));

    // 腮红
    r.ellipse(RectF(cx + f * 7 * sx - 2 * sx, cy + 3 * sy, 5 * sx, 3 * sy),
              rgb(255, 166, 138));

    // 嘴
    Vec2 beakPts[3] = {
        Vec2(cx + f * 14 * sx, cy - 1 * sy),
        Vec2(cx + f * 24 * sx, cy + 2.5f * sy),
        Vec2(cx + f * 14 * sx, cy + 6 * sy)
    };
    r.polygon(beakPts, 3, beak);
    r.line(Vec2(cx + f * 14 * sx, cy + 2.5f * sy),
           Vec2(cx + f * 21 * sx, cy + 2.5f * sy), rgb(214, 98, 28), 1);

    // 胸前的小星星
    float starR = 3.4f * scale;
    Vec2 star[10];
    for (int i = 0; i < 10; ++i) {
        float a = -kPi * 0.5f + i * kPi / 5.0f;
        float rr = (i % 2 == 0) ? starR : starR * 0.45f;
        star[i] = Vec2(cx - f * 1 * sx + std::cos(a) * rr,
                       cy + 10 * sy + std::sin(a) * rr);
    }
    r.polygon(star, 10, rgb(255, 246, 190));
}

} // namespace jubi
