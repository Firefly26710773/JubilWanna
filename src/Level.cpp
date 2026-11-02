// ============================================================
//  Level.cpp  -  6 个单屏关卡、碰撞、机关与场景美术
// ============================================================
#include "Level.h"

#include "Audio.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>

namespace jubi {

namespace {

const RoomDef kRooms[6] = {
    // ---------------- 1 ----------------
    {
        L"摇篮曲",
        L"第一关 · 轻轻地，别掉下去",
        L"「先试试看能不能跳起来吧。」",
        rgb(24, 24, 58), rgb(238, 142, 104), rgb(255, 214, 128),
        rgb(54, 44, 64), rgb(122, 100, 116),
        {
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "..................F.............",
            "................................",
            "................................",
            ".................===............",
            "................................",
            ".............===................",
            "..P..S...^^^.................E..",
            "######################...#######",
            "######################^^^#######",
        },
        {}, {}, {}, 0
    },
    // ---------------- 2 ----------------
    {
        L"断桥",
        L"第二关 · 会碎的路",
        L"「路是假的，掉下去是真的。」",
        rgb(38, 22, 58), rgb(246, 116, 86), rgb(255, 182, 120),
        rgb(58, 38, 52), rgb(150, 94, 98),
        {
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "........................F.......",
            "................................",
            "................................",
            "..............CC..........CC....",
            "..........CC...........CC.......",
            "..P.S.............S...........E.",
            "########.........###........####",
            "########^^^^^^^^^###^^^^^^^^####",
        },
        {}, {}, {}, 1
    },
    // ---------------- 3 ----------------
    {
        L"针雨",
        L"第三关 · 等一等，再跳",
        L"「急什么？尖刺又不会跑。」",
        rgb(8, 28, 36), rgb(18, 66, 78), rgb(120, 240, 230),
        rgb(26, 50, 55), rgb(96, 150, 150),
        {
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            ".................F..............",
            "................................",
            ".............TT..........TT.....",
            ".........TT..........TT.........",
            ".P...............S............E.",
            "#######.........###.........####",
            "#######^^^^^^^^^###^^^^^^^^^####",
        },
        {}, {}, {{12, 5}, {23, 5}}, 2
    },
    // ---------------- 4 ----------------
    {
        L"浮空回廊",
        L"第四关 · 脚下是空的",
        L"「别往下看。」",
        rgb(18, 12, 40), rgb(84, 42, 112), rgb(202, 164, 255),
        rgb(45, 34, 70), rgb(122, 92, 162),
        {
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            ".....................F..........",
            "................................",
            "................................",
            ".....................S..........",
            "....................===.........",
            "................................",
            ".P.S..........................E.",
            "########....................####",
            "########^^^^^^^^^^^^^^^^^^^^####",
        },
        {
            {8.5f * 40, 14 * 40, 14 * 40, 14 * 40, 80, 14, 0.34f, 0.0f},
            {17 * 40, 15 * 40, 17 * 40, 11 * 40, 80, 14, 0.30f, 0.35f},
            {24 * 40, 14 * 40, 26 * 40, 14 * 40, 80, 14, 0.40f, 0.60f},
        },
        {}, {}, 3
    },
    // ---------------- 5 ----------------
    {
        L"逆风",
        L"第五关 · 借风一程",
        L"「风会推你，也会骗你。」",
        rgb(10, 22, 48), rgb(50, 90, 120), rgb(140, 220, 255),
        rgb(32, 44, 62), rgb(100, 130, 160),
        {
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................F...............",
            "................................",
            "................................",
            "..............===..S....===..E..",
            "........====.......===.......===",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            ".PS.B.......^^........^^........",
            "################################",
            "################################",
        },
        {},
        {{12.8f * 40, 8.2f * 40, -1.0f}},
        {}, 4
    },
    // ---------------- 6 ----------------
    {
        L"终幕",
        L"最终关 · 心脏的鼓点",
        L"「都到这里了，别停。」",
        rgb(30, 6, 10), rgb(130, 24, 28), rgb(255, 120, 90),
        rgb(55, 24, 28), rgb(142, 72, 62),
        {
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................................",
            "................F...............",
            "................................",
            "................................",
            "................................",
            "...........CC.......TT..........",
            ".......CC.......................",
            ".PS.............S.............E.",
            "######.........###..........####",
            "######^^^^^^^^^###^^^^^^^^^^####",
        },
        {
            {24 * 40, 13 * 40, 26 * 40, 13 * 40, 80, 14, 0.36f, 0.20f},
        },
        {}, {{11, 4}, {20, 4}}, 5
    },
};

void gradient(Renderer& r, Color top, Color bottom, int bands = 36) {
    float h = static_cast<float>(kScreenH) / bands;
    for (int i = 0; i < bands; ++i) {
        float t = static_cast<float>(i) / (bands - 1);
        r.fillRect(RectF(0, i * h, kScreenW, h + 1.2f), mix(top, bottom, smoothstep(t)));
    }
}

void drawStars(Renderer& r, float time, int count, float top, float bottom,
               Color color, int seed) {
    for (int i = 0; i < count; ++i) {
        float x = hash01(i, 3, seed) * kScreenW;
        float y = top + hash01(i, 7, seed + 1) * (bottom - top);
        float tw = 0.45f + 0.55f * (0.5f + 0.5f * std::sin(time * (0.7f + hash01(i, 9, seed) * 1.8f)
                                                           + i * 1.7f));
        float s = 0.8f + hash01(i, 11, seed) * 1.7f;
        r.circle(x, y, s * tw, mix(color, rgb(255, 255, 255), 0.25f * tw));
    }
}

void drawCloud(Renderer& r, float x, float y, float scale, Color c) {
    r.fillRoundRect(RectF(x - 58 * scale, y - 15 * scale, 116 * scale, 30 * scale),
                    15 * scale, c);
    r.fillRoundRect(RectF(x - 34 * scale, y - 26 * scale, 62 * scale, 26 * scale),
                    13 * scale, brighten(c, 0.04f));
    r.fillRoundRect(RectF(x + 8 * scale, y - 20 * scale, 48 * scale, 22 * scale),
                    11 * scale, c);
}

void drawMountain(Renderer& r, float baseY, float height, float x0, float x1,
                  Color c, float snow = 0.0f) {
    Vec2 pts[3] = {Vec2(x0, baseY), Vec2((x0 + x1) * 0.5f, baseY - height), Vec2(x1, baseY)};
    r.polygon(pts, 3, c);
    if (snow > 0.0f) {
        float t = 0.30f;
        Vec2 cs[3] = {
            Vec2((x0 + x1) * 0.5f - (x1 - x0) * t * 0.5f, baseY - height * (1.0f - t)),
            Vec2((x0 + x1) * 0.5f, baseY - height),
            Vec2((x0 + x1) * 0.5f + (x1 - x0) * t * 0.5f, baseY - height * (1.0f - t))
        };
        r.polygon(cs, 3, mix(c, rgb(255, 255, 255), snow));
    }
}

void drawBirdIcon(Renderer& r, float cx, float cy, float scale, float time, bool active) {
    Color body = active ? rgb(255, 214, 80) : rgb(140, 140, 150);
    Color wing = active ? rgb(255, 240, 160) : rgb(110, 110, 120);
    r.circle(cx, cy, 13 * scale, darken(body, 0.25f));
    r.circle(cx, cy - 1 * scale, 11 * scale, body);
    r.circle(cx - 5 * scale, cy - 3 * scale, 4.5f * scale, wing);
    r.circle(cx + 4 * scale, cy - 4 * scale, 4 * scale, rgb(255, 255, 255));
    r.circle(cx + 5.5f * scale, cy - 4 * scale, 2 * scale, rgb(30, 30, 40));
    Vec2 beak[3] = {
        Vec2(cx + 10 * scale, cy + 1 * scale),
        Vec2(cx + 18 * scale, cy + 3 * scale),
        Vec2(cx + 10 * scale, cy + 6 * scale)
    };
    r.polygon(beak, 3, rgb(255, 140, 50));
    r.line(Vec2(cx - 4 * scale, cy + 10 * scale), Vec2(cx - 4 * scale, cy + 15 * scale),
           rgb(255, 140, 50), iround(2 * scale));
    r.line(Vec2(cx + 4 * scale, cy + 10 * scale), Vec2(cx + 4 * scale, cy + 15 * scale),
           rgb(255, 140, 50), iround(2 * scale));
    (void)time;
}

} // namespace

// ============================================================
//  加载 / 重置
// ============================================================
bool Level::load(int index) {
    if (index < 0) index = 0;
    if (index >= roomCount()) index = roomCount() - 1;
    def_ = &kRooms[index];
    roomIndex_ = index;
    resetRuntime();
    return true;
}

void Level::resetRuntime() {
    int mask = featherMask();
    tiles_.fill(Tile{});
    spawn_ = Vec2(60, 600);
    door_ = RectF{};
    hasDoor_ = false;
    movers_.clear();
    shooters_.clear();
    darts_.clear();
    fallers_.clear();
    checkpoints_.clear();
    feathers_.clear();
    activeCheckpoint_ = -1;
    time_ = 0.0f;
    shootFlash_ = 0.0f;

    parseMap();
    setFeatherMask(mask);
}

void Level::parseMap() {
    for (int r = 0; r < 18; ++r) {
        const char* row = def_->rows[r];
        for (int c = 0; c < 32; ++c) {
            char ch = row[c];
            Tile& t = tiles_[r * 32 + c];
            switch (ch) {
                case '#': t.type = TileType::Solid; break;
                case '=': t.type = TileType::Platform; break;
                case 'C': t.type = TileType::Crumble; break;
                case 'T': t.type = TileType::Timed; break;
                case '^': t.type = TileType::Spike; break;
                case 'B': t.type = TileType::Bounce; break;
                case 'P':
                    spawn_ = Vec2(c * 40 + (40 - kPlayerW) * 0.5f,
                                  (r + 1) * 40.0f - kPlayerH);
                    break;
                case 'S': {
                    Checkpoint cp;
                    cp.tile = Vec2(static_cast<float>(c), static_cast<float>(r));
                    cp.spawn = Vec2(c * 40 + (40 - kPlayerW) * 0.5f,
                                    (r + 1) * 40.0f - kPlayerH);
                    checkpoints_.push_back(cp);
                    break;
                }
                case 'F': {
                    Feather f;
                    f.center = Vec2(c * 40 + 20.0f, r * 40 + 20.0f);
                    f.col = c;
                    f.row = r;
                    feathers_.push_back(f);
                    break;
                }
                case 'E':
                    door_ = RectF(c * 40.0f, r * 40.0f - 40.0f, 40.0f, 80.0f);
                    hasDoor_ = true;
                    break;
                default: break;
            }
        }
    }

    for (const MoverDef& d : def_->movers) {
        Mover m;
        m.a = Vec2(d.x0, d.y0);
        m.b = Vec2(d.x1, d.y1);
        m.w = d.w;
        m.h = d.h;
        m.speed = d.speed;
        m.t = d.phase;
        float k = pingpong01(m.t);
        m.pos = Vec2(lerpf(m.a.x, m.b.x, k), lerpf(m.a.y, m.b.y, k));
        m.prev = m.pos;
        movers_.push_back(m);
    }
    for (const ShooterDef& d : def_->shooters) {
        Shooter s;
        s.pos = Vec2(d.x, d.y);
        s.dir = d.dir;
        s.timer = 0.9f + static_cast<float>(shooters_.size()) * 0.7f;
        shooters_.push_back(s);
    }
    for (const FallerDef& d : def_->fallers) {
        Faller f;
        f.home = Vec2(d.x * 40.0f + 3.0f, d.y * 40.0f);
        f.pos = f.home;
        fallers_.push_back(f);
    }
}

void Level::setCheckpoint(int index) {
    activeCheckpoint_ = -1;
    for (auto& cp : checkpoints_) cp.active = false;
    if (index >= 0 && index < static_cast<int>(checkpoints_.size())) {
        checkpoints_[index].active = true;
        activeCheckpoint_ = index;
    }
}

Vec2 Level::respawnPoint() const {
    if (activeCheckpoint_ >= 0 && activeCheckpoint_ < static_cast<int>(checkpoints_.size())) {
        return checkpoints_[activeCheckpoint_].spawn;
    }
    return spawn_;
}

bool Level::featherTaken(int index) const {
    if (index < 0 || index >= static_cast<int>(feathers_.size())) return false;
    return feathers_[index].taken;
}

void Level::setFeatherMask(int mask) {
    for (size_t i = 0; i < feathers_.size(); ++i) {
        feathers_[i].taken = (mask & (1 << i)) != 0;
    }
}

int Level::featherMask() const {
    int mask = 0;
    for (size_t i = 0; i < feathers_.size() && i < 8; ++i) {
        if (feathers_[i].taken) mask |= (1 << i);
    }
    return mask;
}

int Level::featherCount() const {
    int n = 0;
    for (const auto& f : feathers_) if (f.taken) ++n;
    return n;
}

// ============================================================
//  碰撞
// ============================================================
bool Level::solidAt(int c, int r) const {
    if (c < 0 || c >= 32) return true;
    if (r < 0 || r >= 18) return false;
    return tiles_[r * 32 + c].type == TileType::Solid;
}

bool Level::moveX(RectF& box, float dx) const {
    if (std::fabs(dx) < 0.00001f) return false;
    box.x += dx;
    bool hit = false;
    if (box.x < 0.0f) { box.x = 0.0f; hit = true; }
    if (box.x + box.w > kScreenW) { box.x = kScreenW - box.w; hit = true; }

    int r0 = static_cast<int>(std::floor(box.y / 40.0f));
    int r1 = static_cast<int>(std::floor((box.y + box.h - 0.01f) / 40.0f));
    int c0 = static_cast<int>(std::floor(box.x / 40.0f));
    int c1 = static_cast<int>(std::floor((box.x + box.w - 0.01f) / 40.0f));
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            if (!solidAt(c, r)) continue;
            if (dx > 0.0f) box.x = c * 40.0f - box.w - 0.01f;
            else box.x = (c + 1) * 40.0f + 0.01f;
            hit = true;
            return hit;
        }
    }
    return hit;
}

MoveYResult Level::moveY(RectF& box, float dy) const {
    MoveYResult res;
    if (std::fabs(dy) < 0.00001f) return res;

    float prevTop = box.y;
    float prevBottom = box.y + box.h;
    box.y += dy;

    if (dy > 0.0f) {
        float bestTop = 1.0e9f;
        bool bounce = false;
        int moverIndex = -1;

        int c0 = static_cast<int>(std::floor((box.x + 1.0f) / 40.0f));
        int c1 = static_cast<int>(std::floor((box.x + box.w - 1.0f) / 40.0f));
        int r0 = static_cast<int>(std::floor(prevBottom / 40.0f)) - 1;
        int r1 = static_cast<int>(std::floor((box.y + box.h) / 40.0f)) + 1;
        r0 = (std::max)(0, r0);
        r1 = (std::min)(17, r1);

        for (int r = r0; r <= r1; ++r) {
            for (int c = c0; c <= c1; ++c) {
                if (c < 0 || c >= 32) continue;
                const Tile& t = tiles_[r * 32 + c];
                if (t.type == TileType::Empty || t.type == TileType::Spike) continue;
                float top = r * 40.0f;
                if (prevBottom > top + 4.5f) continue;
                if (box.y + box.h < top) continue;
                if (t.type == TileType::Solid) {
                    if (top < bestTop) { bestTop = top; bounce = false; moverIndex = -1; }
                } else {
                    if (!t.active) continue;
                    if (top < bestTop) {
                        bestTop = top;
                        bounce = (t.type == TileType::Bounce);
                        moverIndex = -1;
                    }
                }
            }
        }

        for (size_t i = 0; i < movers_.size(); ++i) {
            const Mover& m = movers_[i];
            if (box.x + box.w <= m.pos.x || box.x >= m.pos.x + m.w) continue;
            float top = m.pos.y;
            if (prevBottom > top + 4.5f) continue;
            if (box.y + box.h < top) continue;
            if (top < bestTop) {
                bestTop = top;
                bounce = false;
                moverIndex = static_cast<int>(i);
            }
        }

        if (bestTop < 1.0e8f) {
            box.y = bestTop - box.h;
            res.landed = true;
            res.bounce = bounce;
            res.onMover = moverIndex >= 0;
            res.moverIndex = moverIndex;
        }
    } else {
        float bestBottom = -1.0e9f;
        int c0 = static_cast<int>(std::floor((box.x + 1.0f) / 40.0f));
        int c1 = static_cast<int>(std::floor((box.x + box.w - 1.0f) / 40.0f));
        int r0 = static_cast<int>(std::floor(box.y / 40.0f));
        int r1 = static_cast<int>(std::floor((box.y + box.h - 0.01f) / 40.0f));
        for (int r = r0; r <= r1; ++r) {
            for (int c = c0; c <= c1; ++c) {
                if (!solidAt(c, r)) continue;
                float bottom = (r + 1) * 40.0f;
                if (prevTop < bottom - 4.5f) continue;
                if (box.y > bottom) continue;
                if (bottom > bestBottom) bestBottom = bottom;
            }
        }
        if (bestBottom > -1.0e8f) {
            box.y = bestBottom;
            res.landed = true;
        }
    }
    return res;
}

bool Level::touchingBounce(const RectF& box) const {
    int c0 = (std::max)(0, static_cast<int>(std::floor(box.x / 40.0f)) - 1);
    int c1 = (std::min)(31, static_cast<int>(std::floor((box.x + box.w) / 40.0f)) + 1);
    int r0 = (std::max)(0, static_cast<int>(std::floor(box.y / 40.0f)) - 1);
    int r1 = (std::min)(17, static_cast<int>(std::floor((box.y + box.h) / 40.0f)) + 1);
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            if (tiles_[r * 32 + c].type != TileType::Bounce) continue;
            RectF pad(c * 40.0f, r * 40.0f - 10.0f, 40.0f, 50.0f);
            if (pad.intersects(box)) return true;
        }
    }
    return false;
}

bool Level::hazardAt(const RectF& box) const {
    int c0 = (std::max)(0, static_cast<int>(std::floor(box.x / 40.0f)) - 1);
    int c1 = (std::min)(31, static_cast<int>(std::floor((box.x + box.w) / 40.0f)) + 1);
    int r0 = (std::max)(0, static_cast<int>(std::floor(box.y / 40.0f)) - 1);
    int r1 = (std::min)(17, static_cast<int>(std::floor((box.y + box.h) / 40.0f)) + 1);
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            if (tiles_[r * 32 + c].type != TileType::Spike) continue;
            RectF spike(c * 40.0f + 5.0f, r * 40.0f + 15.0f, 30.0f, 25.0f);
            if (spike.intersects(box)) return true;
        }
    }
    for (const Dart& d : darts_) {
        if (!d.alive) continue;
        if (RectF(d.pos.x - 8.0f, d.pos.y - 3.0f, 16.0f, 6.0f).intersects(box)) return true;
    }
    for (const Faller& f : fallers_) {
        if (f.warn > 0.0f && !f.falling) continue;
        if (RectF(f.pos.x, f.pos.y, 34.0f, 34.0f).intersects(box)) return true;
    }
    return false;
}

bool Level::goalAt(const RectF& box) const {
    return hasDoor_ && door_.intersects(box);
}

int Level::collectFeather(const RectF& box) {
    for (size_t i = 0; i < feathers_.size(); ++i) {
        Feather& f = feathers_[i];
        if (f.taken) continue;
        RectF hit(f.center.x - 18.0f, f.center.y - 18.0f, 36.0f, 36.0f);
        if (hit.intersects(box)) {
            f.taken = true;
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Level::touchCheckpoint(const RectF& box) {
    for (size_t i = 0; i < checkpoints_.size(); ++i) {
        Checkpoint& cp = checkpoints_[i];
        if (cp.active) continue;
        RectF hit(cp.tile.x * 40.0f + 8.0f, cp.tile.y * 40.0f - 10.0f, 24.0f, 60.0f);
        if (hit.intersects(box)) {
            for (auto& other : checkpoints_) other.active = false;
            cp.active = true;
            activeCheckpoint_ = static_cast<int>(i);
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Level::triggerCrumbleUnder(const RectF& box) {
    int c = static_cast<int>(std::floor((box.x + box.w * 0.5f) / 40.0f));
    int r = static_cast<int>(std::floor((box.y + box.h + 2.0f) / 40.0f));
    for (int dc = -1; dc <= 1; ++dc) {
        int cc = c + dc;
        if (cc < 0 || cc >= 32 || r < 0 || r >= 18) continue;
        Tile& t = tiles_[r * 32 + cc];
        if (t.type == TileType::Crumble && t.active && !t.triggered) {
            t.triggered = true;
            t.timer = 0.62f;
            return;
        }
    }
}

Vec2 Level::moverDelta(int index) const {
    if (index < 0 || index >= static_cast<int>(movers_.size())) return Vec2{};
    return Vec2(movers_[index].deltaX, movers_[index].deltaY);
}

RectF Level::moverRect(int index) const {
    if (index < 0 || index >= static_cast<int>(movers_.size())) return RectF{};
    const Mover& m = movers_[index];
    return RectF(m.pos.x, m.pos.y, m.w, m.h);
}

// ============================================================
//  每帧更新
// ============================================================
float Level::timedPhase(int c, int r) const {
    float p = std::fmod(time_ + c * 0.31f + r * 0.17f, 3.6f);
    if (p < 0.0f) p += 3.6f;
    return p;
}

bool Level::timedActive(int c, int r) const {
    return timedPhase(c, r) < 2.25f;
}

void Level::update(float dt, const RectF& playerBox, bool playerGrounded, Audio& audio) {
    time_ += dt;
    if (shootFlash_ > 0.0f) shootFlash_ -= dt;

    for (int r = 0; r < 18; ++r) {
        for (int c = 0; c < 32; ++c) {
            Tile& t = tiles_[r * 32 + c];
            if (t.type == TileType::Timed) {
                t.active = timedActive(c, r);
            } else if (t.type == TileType::Crumble) {
                if (t.triggered) {
                    t.timer -= dt;
                    if (t.timer <= 0.0f) {
                        t.active = false;
                        t.triggered = false;
                        t.timer = 2.3f;
                    }
                } else if (!t.active) {
                    t.timer -= dt;
                    if (t.timer <= 0.0f) {
                        t.active = true;
                        t.timer = 0.0f;
                    }
                }
            }
        }
    }

    if (playerGrounded) {
        int c = static_cast<int>(std::floor((playerBox.x + playerBox.w * 0.5f) / 40.0f));
        int r = static_cast<int>(std::floor((playerBox.y + playerBox.h + 2.0f) / 40.0f));
        for (int dc = -1; dc <= 1; ++dc) {
            int cc = c + dc;
            if (cc < 0 || cc >= 32 || r < 0 || r >= 18) continue;
            Tile& t = tiles_[r * 32 + cc];
            if (t.type == TileType::Crumble && t.active && !t.triggered) {
                t.triggered = true;
                t.timer = 0.62f;
                audio.play(Sfx::Crumble);
                break;
            }
        }
    }

    for (auto& m : movers_) {
        m.prev = m.pos;
        m.t += m.speed * dt;
        float k = pingpong01(m.t);
        m.pos = Vec2(lerpf(m.a.x, m.b.x, k), lerpf(m.a.y, m.b.y, k));
        m.deltaX = m.pos.x - m.prev.x;
        m.deltaY = m.pos.y - m.prev.y;
    }

    for (auto& s : shooters_) {
        if (s.charging) {
            s.charge -= dt;
            if (s.charge <= 0.0f) {
                s.charging = false;
                s.timer = 2.5f;
                Dart d;
                d.pos = Vec2(s.pos.x + s.dir * 20.0f, s.pos.y);
                d.vel = Vec2(s.dir * 430.0f, 0.0f);
                darts_.push_back(d);
                shootFlash_ = 0.12f;
                audio.play(Sfx::Shoot);
            }
        } else {
            s.timer -= dt;
            if (s.timer <= 0.0f) {
                s.charging = true;
                s.charge = 0.62f;
            }
        }
    }

    for (auto& d : darts_) {
        if (!d.alive) continue;
        d.pos += d.vel * dt;
        int c = static_cast<int>(std::floor(d.pos.x / 40.0f));
        int r = static_cast<int>(std::floor(d.pos.y / 40.0f));
        if (c < 0 || c >= 32 || r < 0 || r >= 18) d.alive = false;
        else if (solidAt(c, r)) d.alive = false;
        else if (d.pos.x < -40.0f || d.pos.x > kScreenW + 40.0f) d.alive = false;
    }
    for (size_t i = 0; i < darts_.size();) {
        if (!darts_[i].alive) darts_.erase(darts_.begin() + static_cast<long long>(i));
        else ++i;
    }

    for (auto& f : fallers_) {
        if (f.settled) {
            f.rest -= dt;
            if (f.rest <= 0.0f) {
                f.pos = f.home;
                f.vy = 0.0f;
                f.falling = false;
                f.settled = false;
                f.warn = 0.0f;
            }
            continue;
        }
        if (f.falling) {
            f.vy += 2400.0f * dt;
            f.pos.y += f.vy * dt;
            int c = (std::max)(0, (std::min)(31, static_cast<int>((f.pos.x + 17.0f) / 40.0f)));
            float groundY = 720.0f;
            for (int r = 0; r < 18; ++r) {
                if (solidAt(c, r)) { groundY = r * 40.0f; break; }
            }
            if (f.pos.y + 34.0f >= groundY) {
                f.pos.y = groundY - 34.0f;
                f.falling = false;
                f.settled = true;
                f.rest = 1.1f;
            } else if (f.pos.y > 760.0f) {
                f.pos = f.home;
                f.falling = false;
                f.vy = 0.0f;
            }
            continue;
        }
        if (f.warn > 0.0f) {
            f.warn -= dt;
            if (f.warn <= 0.0f) {
                f.falling = true;
                f.vy = 160.0f;
            }
            continue;
        }
        float dx = (playerBox.x + playerBox.w * 0.5f) - (f.home.x + 17.0f);
        if (std::fabs(dx) < 95.0f && playerBox.y > f.home.y - 30.0f &&
            playerBox.y < 640.0f) {
            f.warn = 0.45f;
        }
    }
}

// ============================================================
//  绘制：背景
// ============================================================
void Level::drawBackdrop(Renderer& r, float time) const {
    gradient(r, def_->skyTop, def_->skyBottom);
    drawBackdropTheme(r, time);
    // Keep pits readable without painting a hard black floor across the room.
    for (int i = 0; i < 8; ++i) {
        r.alphaRect(RectF(0, 640.0f + i * 10.0f, kScreenW, 10.0f),
                    rgb(0, 0, 0), 18 + i * 22);
    }
    r.alphaRect(RectF(0, 0, kScreenW, 90), rgb(0, 0, 0), 45);
}

void Level::drawBackdropTheme(Renderer& r, float time) const {
    Color sky = def_->skyTop;
    Color accent = def_->accent;

    if (def_->theme == 0) {
        drawStars(r, time, 80, 40, 440, rgb(220, 226, 255), 11);
        r.circle(1050, 150, 86, mix(sky, rgb(255, 220, 150), 0.10f));
        r.circle(1050, 150, 68, mix(sky, rgb(255, 225, 165), 0.16f));
        r.circle(1050, 150, 52, rgb(255, 236, 190));
        r.circle(1038, 138, 9, rgb(238, 216, 170));
        r.circle(1068, 164, 6, rgb(242, 222, 176));
        for (int i = 0; i < 5; ++i) {
            float x = std::fmod(time * (6.0f + i * 2.0f) + i * 280.0f, 1500.0f) - 110.0f;
            drawCloud(r, x, 300 + i * 46, 0.7f + i * 0.11f,
                      mix(def_->skyBottom, rgb(70, 54, 92), 0.45f + i * 0.06f));
        }
        drawMountain(r, 620, 240, -80, 430, mix(sky, rgb(44, 36, 70), 0.55f));
        drawMountain(r, 640, 200, 260, 720, mix(sky, rgb(58, 44, 78), 0.50f), 0.20f);
        drawMountain(r, 660, 260, 600, 1080, mix(sky, rgb(40, 34, 64), 0.62f));
        drawMountain(r, 680, 180, 960, 1400, mix(sky, rgb(50, 40, 72), 0.55f));
        for (int i = 0; i < 18; ++i) {
            float x = hash01(i, 21, 5) * kScreenW;
            float y = 380 + hash01(i, 23, 5) * 240;
            float a = 0.4f + 0.6f * (0.5f + 0.5f * std::sin(time * 1.7f + i));
            r.circle(x, y, 1.6f * a, mix(accent, rgb(255, 255, 255), 0.3f));
        }
    } else if (def_->theme == 1) {
        drawStars(r, time, 35, 30, 300, rgb(255, 220, 190), 21);
        r.circle(980, 300, 150, mix(sky, rgb(255, 150, 90), 0.16f));
        r.circle(980, 300, 118, mix(sky, rgb(255, 170, 100), 0.28f));
        r.circle(980, 300, 88, rgb(255, 210, 140));
        r.circle(968, 286, 14, rgb(248, 190, 120));
        r.circle(1002, 318, 9, rgb(246, 186, 116));
        for (int i = 0; i < 7; ++i) {
            float x = std::fmod(time * (8.0f + i * 2.5f) + i * 240.0f, 1600.0f) - 140.0f;
            drawCloud(r, x, 170 + i * 38, 0.55f + i * 0.12f,
                      mix(rgb(255, 172, 130), rgb(92, 52, 82), 0.35f + i * 0.06f));
        }
        drawMountain(r, 640, 210, -100, 380, mix(sky, rgb(86, 40, 64), 0.65f));
        drawMountain(r, 660, 280, 260, 760, mix(sky, rgb(72, 34, 58), 0.60f), 0.15f);
        drawMountain(r, 670, 200, 640, 1080, mix(sky, rgb(92, 44, 66), 0.62f));
        drawMountain(r, 690, 240, 960, 1400, mix(sky, rgb(66, 30, 54), 0.68f));
        for (int i = 0; i < 5; ++i) {
            float x = 180.0f + i * 240.0f + std::sin(time * 0.4f + i) * 20.0f;
            float y = 120.0f + i * 34.0f;
            r.line(Vec2(x - 12, y), Vec2(x, y + 4), rgb(48, 30, 48), 2);
            r.line(Vec2(x, y + 4), Vec2(x + 12, y), rgb(48, 30, 48), 2);
        }
        for (int i = 0; i < 40; ++i) {
            float x = hash01(i, 5, 9) * kScreenW;
            float y = 380 + hash01(i, 8, 9) * 260;
            r.circle(x, y, 1.2f, mix(def_->skyBottom, rgb(255, 230, 180), 0.6f));
        }
    } else if (def_->theme == 2) {
        drawStars(r, time, 25, 20, 260, rgb(150, 240, 220), 31);
        r.circle(210, 130, 78, mix(sky, rgb(40, 110, 120), 0.22f));
        r.circle(210, 130, 56, mix(sky, rgb(60, 150, 150), 0.25f));
        for (int i = 0; i < 12; ++i) {
            float x = i * 112.0f + 30.0f;
            float h = 70.0f + hash01(i, 3, 4) * 160.0f;
            Vec2 pts[3] = {Vec2(x - 42, 0), Vec2(x, h), Vec2(x + 42, 0)};
            r.polygon(pts, 3, mix(def_->skyTop, rgb(8, 18, 24), 0.45f));
            r.line(Vec2(x, h - 6), Vec2(x, h), mix(accent, rgb(255, 255, 255), 0.2f), 2);
        }
        for (int i = 0; i < 7; ++i) {
            float x = 90.0f + i * 185.0f;
            float y = 530.0f + hash01(i, 6, 4) * 80.0f;
            Vec2 pts[3] = {Vec2(x, y - 54), Vec2(x - 20, y + 8), Vec2(x + 20, y + 8)};
            r.polygon(pts, 3, mix(accent, def_->skyBottom, 0.55f));
            r.line(Vec2(x, y - 44), Vec2(x, y), mix(accent, rgb(255, 255, 255), 0.5f), 2);
        }
        for (int i = 0; i < 5; ++i) {
            float y = 330.0f + i * 54.0f;
            r.alphaRect(RectF(0, y, kScreenW, 22), mix(def_->skyBottom, rgb(120, 220, 210), 0.3f), 12);
        }
        for (int i = 0; i < 32; ++i) {
            float x = hash01(i, 17, 6) * kScreenW;
            float y = 200 + hash01(i, 19, 6) * 420;
            float a = 0.3f + 0.7f * (0.5f + 0.5f * std::sin(time * 1.3f + i));
            r.circle(x, y, 1.4f * a, accent);
        }
    } else if (def_->theme == 3) {
        drawStars(r, time, 120, 20, 620, rgb(232, 220, 255), 41);
        r.circle(1010, 190, 150, mix(sky, rgb(110, 60, 150), 0.18f));
        r.circle(1010, 190, 108, mix(sky, rgb(130, 76, 168), 0.26f));
        r.circle(1010, 190, 82, mix(rgb(120, 76, 160), rgb(40, 30, 70), 0.30f));
        r.arc(1010, 190, 126, 0.15f, 2.9f, mix(accent, rgb(255, 255, 255), 0.2f), 3);
        for (int i = 0; i < 7; ++i) {
            float x = hash01(i, 31, 8) * kScreenW;
            float y = 380 + hash01(i, 33, 8) * 220;
            float w = 40 + hash01(i, 35, 8) * 90;
            Vec2 pts[5] = {
                Vec2(x, y), Vec2(x + w, y - 18), Vec2(x + w + 22, y + 12),
                Vec2(x + w * 0.5f, y + 30), Vec2(x - 8, y + 12)
            };
            r.polygon(pts, 5, mix(def_->skyTop, rgb(70, 50, 100), 0.45f));
        }
        for (int i = 0; i < 6; ++i) {
            float x = std::fmod(time * (4.0f + i * 1.3f) + i * 270.0f, 1500.0f) - 100.0f;
            float y = 260 + i * 50;
            r.alphaRect(RectF(x, y, 260, 70), mix(rgb(130, 90, 190), rgb(40, 24, 70), 0.4f), 10);
        }
    } else if (def_->theme == 4) {
        drawStars(r, time, 20, 20, 220, rgb(190, 220, 255), 51);
        for (int i = 0; i < 8; ++i) {
            float x = std::fmod(time * (10.0f + i * 3.0f) + i * 220.0f, 1600.0f) - 140.0f;
            drawCloud(r, x, 80 + i * 32, 0.9f + i * 0.1f,
                      mix(rgb(38, 62, 92), rgb(14, 28, 52), 0.35f + i * 0.05f));
        }
        float flash = std::fmod(time, 5.3f);
        if (flash < 0.10f) {
            r.alphaRect(RectF(0, 0, kScreenW, kScreenH), rgb(200, 225, 255), 26);
            r.polygon(std::array<Vec2, 6>{
                Vec2(760, 0), Vec2(700, 170), Vec2(744, 170), Vec2(676, 350), Vec2(772, 140),
                Vec2(726, 140)
            }.data(), 6, rgb(210, 235, 255));
        }
        drawMountain(r, 640, 190, -60, 420, mix(sky, rgb(18, 34, 54), 0.55f));
        drawMountain(r, 660, 250, 300, 820, mix(sky, rgb(22, 38, 60), 0.6f));
        drawMountain(r, 680, 170, 780, 1340, mix(sky, rgb(16, 30, 48), 0.65f));
        for (int i = 0; i < 3; ++i) {
            r.alphaRect(RectF(0, 300 + i * 90, kScreenW, 26),
                        mix(def_->skyBottom, rgb(140, 200, 230), 0.25f), 12);
        }
    } else {
        drawStars(r, time, 30, 20, 280, rgb(255, 170, 120), 61);
        float pulse = 0.5f + 0.5f * std::sin(time * 1.4f);
        r.circle(640, 330, 300, mix(rgb(40, 8, 12), rgb(90, 18, 22), 0.35f + pulse * 0.25f));
        r.circle(640, 330, 232, mix(rgb(70, 14, 18), rgb(150, 30, 34), 0.30f + pulse * 0.2f));
        for (int i = 0; i < 12; ++i) {
            float a = i * kPi / 6.0f + time * 0.04f;
            Vec2 p0(640 + std::cos(a) * 150, 330 + std::sin(a) * 150);
            Vec2 p1(640 + std::cos(a) * 260, 330 + std::sin(a) * 260);
            r.line(p0, p1, mix(rgb(90, 24, 26), rgb(230, 80, 70), pulse * 0.5f), 8);
        }
        r.circle(640, 330, 142, rgb(22, 6, 8));
        r.circle(640, 330, 118, mix(rgb(80, 16, 20), rgb(170, 40, 38), pulse * 0.35f));
        for (int i = 0; i < 9; ++i) {
            float x = 100.0f + i * 140.0f;
            float h = 90.0f + hash01(i, 41, 7) * 130.0f;
            Vec2 pts[3] = {Vec2(x - 34, 0), Vec2(x, h), Vec2(x + 34, 0)};
            r.polygon(pts, 3, mix(sky, rgb(12, 4, 6), 0.55f));
        }
        for (int i = 0; i < 22; ++i) {
            float x = hash01(i, 43, 7) * kScreenW;
            float y = 300 + hash01(i, 45, 7) * 300;
            float a = 0.4f + 0.6f * (0.5f + 0.5f * std::sin(time * 2.0f + i));
            r.circle(x, y, 1.8f * a, mix(accent, rgb(255, 220, 140), 0.4f));
        }
    }
}

// ============================================================
//  绘制：世界
// ============================================================
void Level::drawTile(Renderer& rnd, int c, int r, float time) const {
    const Tile& t = tiles_[r * 32 + c];
    if (t.type == TileType::Empty) return;
    float x = c * 40.0f;
    float y = r * 40.0f;
    Color dark = def_->tileDark;
    Color light = def_->tileLight;
    Color accent = def_->accent;

    if (t.type == TileType::Solid) {
        Color base = mix(dark, light, 0.36f);
        base = mix(base, rgb(0, 0, 0), hash01(c, r, 7) * 0.10f);
        rnd.fillRect(RectF(x, y, 40, 40), base);
        rnd.fillRect(RectF(x, y + 31, 40, 9), darken(base, 0.16f));
        bool surface = !solidAt(c, r - 1);
        if (surface) {
            rnd.fillRect(RectF(x, y, 40, 7), light);
            rnd.fillRect(RectF(x, y + 7, 40, 3), darken(light, 0.28f));
            for (int i = 0; i < 3; ++i) {
                float gx = x + 5 + i * 13 + hash01(c, r, i) * 4;
                rnd.line(Vec2(gx, y + 2), Vec2(gx + 2, y - 5), brighten(light, 0.24f), 2);
            }
        }
        rnd.line(Vec2(x + 1, y + 15), Vec2(x + 39, y + 15), darken(base, 0.26f), 1);
        rnd.line(Vec2(x + 1, y + 29), Vec2(x + 39, y + 29), darken(base, 0.26f), 1);
        int off = (r % 2 == 0) ? 20 : 0;
        rnd.line(Vec2(x + off, y + 7), Vec2(x + off, y + 15), darken(base, 0.22f), 1);
        rnd.line(Vec2(x + 20 - off, y + 15), Vec2(x + 20 - off, y + 29), darken(base, 0.22f), 1);
        rnd.line(Vec2(x + off, y + 29), Vec2(x + off, y + 40), darken(base, 0.22f), 1);
        rnd.fillRect(RectF(x, y, 2, 40), mix(base, rgb(0, 0, 0), 0.18f));
        rnd.fillRect(RectF(x + 38, y, 2, 40), mix(base, rgb(255, 255, 255), 0.10f));
    } else if (t.type == TileType::Spike) {
        rnd.fillRect(RectF(x, y + 30, 40, 10), mix(dark, rgb(0, 0, 0), 0.25f));
        for (int i = 0; i < 4; ++i) {
            float bx = x + 3 + i * 9.5f;
            Vec2 pts[3] = {Vec2(bx, y + 40), Vec2(bx + 4.8f, y + 14), Vec2(bx + 9.5f, y + 40)};
            rnd.polygon(pts, 3, mix(light, rgb(230, 240, 255), 0.35f));
            Vec2 inner[3] = {
                Vec2(bx + 2.2f, y + 38), Vec2(bx + 4.8f, y + 22), Vec2(bx + 7.2f, y + 38)
            };
            rnd.polygon(inner, 3, mix(dark, rgb(255, 255, 255), 0.28f));
        }
    } else if (t.type == TileType::Platform) {
        Color plank = mix(light, accent, 0.12f);
        rnd.fillRect(RectF(x, y + 12, 40, 4), mix(dark, rgb(0, 0, 0), 0.25f));
        rnd.fillRoundRect(RectF(x, y, 40, 12), 4, plank);
        rnd.fillRect(RectF(x + 1, y + 1, 38, 3), brighten(plank, 0.20f));
        rnd.line(Vec2(x + 8, y + 12), Vec2(x + 8, y + 21), darken(plank, 0.40f), 2);
        rnd.line(Vec2(x + 32, y + 12), Vec2(x + 32, y + 21), darken(plank, 0.40f), 2);
        rnd.circle(x + 8, y + 22, 2, accent);
        rnd.circle(x + 32, y + 22, 2, accent);
    } else if (t.type == TileType::Crumble) {
        float shake = t.triggered ? std::sin(time * 90.0f) * 2.6f : 0.0f;
        Color stone = mix(dark, light, 0.45f);
        rnd.fillRect(RectF(x + shake, y + 13, 40, 3), mix(dark, rgb(0, 0, 0), 0.3f));
        rnd.fillRoundRect(RectF(x + shake, y, 40, 13), 4, stone);
        rnd.fillRect(RectF(x + shake + 1, y + 1, 38, 3), brighten(stone, 0.16f));
        Color crack = darken(stone, 0.45f);
        rnd.line(Vec2(x + shake + 6, y + 1), Vec2(x + shake + 10, y + 7), crack, 1);
        rnd.line(Vec2(x + shake + 10, y + 7), Vec2(x + shake + 8, y + 13), crack, 1);
        rnd.line(Vec2(x + shake + 25, y + 1), Vec2(x + shake + 21, y + 6), crack, 1);
        rnd.line(Vec2(x + shake + 21, y + 6), Vec2(x + shake + 26, y + 13), crack, 1);
        if (t.triggered) {
            rnd.alphaRect(RectF(x + shake, y - 2, 40, 17), rgb(255, 120, 80), 35);
        }
    } else if (t.type == TileType::Timed) {
        float phase = timedPhase(c, r);
        bool active = t.active;
        bool warn = phase > 1.82f;
        Color glow = mix(accent, rgb(255, 255, 255), 0.15f);
        if (active) {
            float pulse = 0.5f + 0.5f * std::sin(time * 5.0f + c);
            rnd.fillRoundRect(RectF(x, y - 2, 40, 16), 5, mix(dark, glow, 0.25f + pulse * 0.12f));
            rnd.fillRoundRect(RectF(x + 1, y, 38, 11), 4, mix(glow, dark, 0.25f));
            rnd.fillRect(RectF(x + 2, y + 1, 36, 3), mix(glow, rgb(255, 255, 255), 0.4f));
            for (int i = 0; i < 3; ++i) {
                rnd.circle(x + 10 + i * 10, y + 8, 1.6f, rgb(255, 255, 255));
            }
            if (warn) {
                float blink = std::fmod(time * 18.0f, 2.0f) < 1.0f ? 0.7f : 0.25f;
                rnd.strokeRoundRect(RectF(x, y - 2, 40, 16), 5,
                                  mix(rgb(255, 120, 80), glow, 1.0f - blink), 2);
            }
        } else {
            Color ghost = mix(dark, def_->skyTop, 0.55f);
            rnd.fillRoundRect(RectF(x + 2, y + 3, 36, 7), 3, ghost);
            rnd.strokeRoundRect(RectF(x, y - 1, 40, 14), 5, mix(ghost, accent, 0.25f), 1);
        }
    } else if (t.type == TileType::Bounce) {
        Color pad = mix(accent, rgb(255, 255, 255), 0.15f);
        rnd.fillRect(RectF(x + 4, y + 31, 32, 9), mix(dark, rgb(0, 0, 0), 0.25f));
        rnd.line(Vec2(x + 9, y + 31), Vec2(x + 15, y + 22), darken(pad, 0.35f), 3);
        rnd.line(Vec2(x + 15, y + 22), Vec2(x + 22, y + 31), darken(pad, 0.35f), 3);
        rnd.line(Vec2(x + 22, y + 31), Vec2(x + 29, y + 22), darken(pad, 0.35f), 3);
        float hop = std::sin(time * 4.0f + c) * 1.5f;
        rnd.fillRoundRect(RectF(x + 3, y + 14 + hop, 34, 9), 4, pad);
        rnd.fillRect(RectF(x + 5, y + 15 + hop, 30, 3), brighten(pad, 0.35f));
        rnd.circle(x + 20, y + 18 + hop, 2, rgb(255, 255, 255));
    }
}

void Level::drawMover(Renderer& r, const Mover& m, float time) const {
    Color plank = mix(def_->tileLight, def_->accent, 0.30f);
    r.fillRoundRect(RectF(m.pos.x + 3, m.pos.y + 12, m.w, 6), 3, mix(def_->tileDark, rgb(0, 0, 0), 0.3f));
    r.fillRoundRect(RectF(m.pos.x, m.pos.y, m.w, 13), 6, plank);
    r.fillRect(RectF(m.pos.x + 3, m.pos.y + 2, m.w - 6, 3), brighten(plank, 0.30f));
    r.line(Vec2(m.pos.x + 10, m.pos.y + 13), Vec2(m.pos.x + 10, m.pos.y + 20),
           darken(plank, 0.35f), 2);
    r.line(Vec2(m.pos.x + m.w - 10, m.pos.y + 13), Vec2(m.pos.x + m.w - 10, m.pos.y + 20),
           darken(plank, 0.35f), 2);
    float pulse = 0.5f + 0.5f * std::sin(time * 3.0f + m.t * 4.0f);
    r.circle(m.pos.x + 14, m.pos.y + 6, 2.4f, mix(def_->accent, rgb(255, 255, 255), pulse * 0.6f));
    r.circle(m.pos.x + m.w - 14, m.pos.y + 6, 2.4f,
             mix(def_->accent, rgb(255, 255, 255), (1.0f - pulse) * 0.6f));
    r.line(Vec2(m.pos.x + m.w * 0.5f, m.pos.y + 14),
           Vec2(m.pos.x + m.w * 0.5f, m.pos.y + 26), def_->accent, 2);
    Vec2 tip[3] = {
        Vec2(m.pos.x + m.w * 0.5f - 5, m.pos.y + 24),
        Vec2(m.pos.x + m.w * 0.5f, m.pos.y + 31),
        Vec2(m.pos.x + m.w * 0.5f + 5, m.pos.y + 24)
    };
    r.polygon(tip, 3, def_->accent);
}

void Level::drawShooter(Renderer& r, const Shooter& s, float time) const {
    float bob = std::sin(time * 2.0f + s.pos.x * 0.02f) * 3.0f;
    float cx = s.pos.x;
    float cy = s.pos.y + bob;
    if (s.charging) {
        float t = 1.0f - s.charge / 0.62f;
        Color warn = mix(rgb(255, 210, 120), rgb(255, 70, 60), t);
        for (int i = 0; i < 8; ++i) {
            float d0 = 24.0f + i * 34.0f;
            float d1 = d0 + 20.0f;
            float dx0 = cx + s.dir * d0;
            float dx1 = cx + s.dir * d1;
            r.line(Vec2(dx0, cy), Vec2(dx1, cy), warn, 2);
        }
        r.alphaRect(RectF(cx - 20, cy - 20, 40, 40), warn, 22);
    }
    r.line(Vec2(cx - 14, cy - 10), Vec2(cx - 23, cy - 18), def_->tileLight, 3);
    r.line(Vec2(cx + 14, cy - 10), Vec2(cx + 23, cy - 18), def_->tileLight, 3);
    r.circle(cx, cy, 16, mix(def_->tileDark, rgb(0, 0, 0), 0.25f));
    r.circle(cx, cy, 13, mix(def_->tileDark, def_->accent, 0.22f));
    r.circle(cx, cy, 9, mix(def_->tileDark, rgb(0, 0, 0), 0.15f));
    r.circle(cx + s.dir * 3.5f, cy, 5.5f,
             s.charging ? rgb(255, 100, 70) : mix(def_->accent, rgb(255, 255, 255), 0.3f));
    r.circle(cx + s.dir * 4.8f, cy - 1.5f, 1.8f, rgb(255, 255, 255));
    r.line(Vec2(cx, cy - 16), Vec2(cx, cy - 24), def_->accent, 2);
    r.circle(cx, cy - 26, 2.5f, mix(def_->accent, rgb(255, 255, 255), 0.5f));
}

void Level::drawFaller(Renderer& r, const Faller& f, float time) const {
    float shake = (f.warn > 0.0f && !f.falling) ? std::sin(time * 80.0f) * 3.0f : 0.0f;
    float x = f.pos.x + shake;
    float y = f.pos.y;
    if (!f.falling && !f.settled) {
        r.line(Vec2(x + 17, 0), Vec2(x + 17, y), mix(def_->tileLight, rgb(0, 0, 0), 0.4f), 2);
    }
    if (f.warn > 0.0f && !f.falling) {
        r.alphaRect(RectF(x - 4, y - 4, 42, 42), rgb(255, 90, 70), 35);
    }
    r.fillRoundRect(RectF(x + 2, y + 2, 30, 28), 4, mix(def_->tileDark, rgb(0, 0, 0), 0.2f));
    r.fillRect(RectF(x + 4, y + 4, 26, 4), mix(def_->tileLight, rgb(255, 255, 255), 0.2f));
    for (int i = 0; i < 3; ++i) {
        Vec2 pts[3] = {
            Vec2(x + 5 + i * 9.0f, y + 30), Vec2(x + 9.5f + i * 9.0f, y + 42),
            Vec2(x + 14 + i * 9.0f, y + 30)
        };
        r.polygon(pts, 3, rgb(220, 225, 235));
    }
    r.circle(x + 17, y + 16, 3.0f, rgb(255, 80, 70));
}

void Level::drawCheckpoint(Renderer& r, const Checkpoint& cp, float time) const {
    float cx = cp.tile.x * 40.0f + 20.0f;
    float base = (cp.tile.y + 1) * 40.0f;
    float top = cp.tile.y * 40.0f - 26.0f;
    Color pole = cp.active ? rgb(240, 236, 220) : rgb(120, 118, 126);
    Color flag = cp.active ? def_->accent : rgb(96, 94, 104);
    r.line(Vec2(cx, top), Vec2(cx, base), pole, 3);
    r.circle(cx, top - 3, 4, mix(pole, rgb(255, 255, 255), 0.3f));
    if (cp.active) {
        r.circle(cx, top + 2, 14, mix(flag, def_->skyTop, 0.65f));
        r.circle(cx, top + 2, 9, mix(flag, rgb(255, 255, 255), 0.2f));
        r.circle(cx, top + 2, 5, rgb(255, 255, 240));
    }
    float wave = cp.active ? std::sin(time * 3.2f) * 5.0f : 0.0f;
    Vec2 pts[3] = {
        Vec2(cx + 1, top + 4),
        Vec2(cx + 27 + wave, top + 13),
        Vec2(cx + 1, top + 22)
    };
    r.polygon(pts, 3, flag);
    r.line(Vec2(cx + 1, top + 4), Vec2(cx + 1, base), cp.active ? brighten(flag, 0.3f) : pole, 1);
    if (cp.active) {
        for (int i = 0; i < 3; ++i) {
            float a = time * 2.2f + i * 2.1f;
            float px = cx + std::cos(a) * (10.0f + i * 4.0f);
            float py = top + 4 + std::sin(a * 1.3f) * (8.0f + i * 3.0f);
            r.circle(px, py, 1.5f, mix(flag, rgb(255, 255, 255), 0.6f));
        }
    }
}

void Level::drawFeather(Renderer& r, const Feather& f, float time) const {
    if (f.taken) return;
    float bob = std::sin(time * 2.2f + f.col * 0.7f) * 4.0f;
    float cx = f.center.x;
    float cy = f.center.y + bob;
    float pulse = 0.5f + 0.5f * std::sin(time * 3.0f + f.row);
    r.circle(cx, cy, 22, mix(def_->accent, def_->skyTop, 0.55f));
    r.circle(cx, cy, 16, mix(def_->accent, def_->skyTop, 0.35f));
    r.circle(cx, cy, 10, mix(def_->accent, rgb(255, 255, 255), 0.35f + pulse * 0.2f));
    float ang = std::sin(time * 1.6f + f.col) * 0.45f;
    auto rot = [&](float x, float y) {
        float ca = std::cos(ang);
        float sa = std::sin(ang);
        return Vec2(cx + x * ca - y * sa, cy + x * sa + y * ca);
    };
    Vec2 pts[4] = {rot(0, -15), rot(9, -2), rot(0, 15), rot(-7, 0)};
    r.polygon(pts, 4, rgb(255, 224, 120));
    r.polygon(std::array<Vec2, 4>{rot(0, -11), rot(5, -2), rot(0, 8), rot(-4, 0)}.data(), 4,
              rgb(255, 245, 190));
    r.line(rot(0, -13), rot(0, 14), rgb(255, 170, 80), 2);
    for (int i = 0; i < 4; ++i) {
        float a = time * 2.4f + i * 1.57f;
        r.circle(cx + std::cos(a) * 24, cy + std::sin(a) * 18, 1.8f, rgb(255, 240, 180));
    }
}

void Level::drawDoor(Renderer& r, float time) const {
    if (!hasDoor_) return;
    float x = door_.x;
    float top = door_.y;
    float bottom = door_.y + door_.h;
    float cx = x + 20;
    float pulse = 0.5f + 0.5f * std::sin(time * 2.6f);
    r.alphaRect(RectF(x - 14, top - 14, 68, door_.h + 28), def_->accent, 20 + static_cast<int>(pulse * 12));
    r.fillRoundRect(RectF(x - 3, top - 3, 46, door_.h + 6), 19, mix(def_->tileDark, rgb(0, 0, 0), 0.2f));
    r.fillRoundRect(RectF(x, top, 40, door_.h), 17, mix(def_->tileDark, def_->accent, 0.15f));
    r.fillRoundRect(RectF(x + 5, top + 6, 30, door_.h - 12), 13, rgb(12, 12, 22));
    for (int i = 0; i < 3; ++i) {
        float t = i / 3.0f;
        r.fillRoundRect(RectF(x + 7 + i * 4, top + 10 + i * 4, 26 - i * 8, door_.h - 20 - i * 8),
                        9, mix(rgb(20, 20, 40), def_->accent, 0.10f + pulse * 0.12f + t * 0.08f));
    }
    r.strokeRoundRect(RectF(x - 4, top - 4, 48, door_.h + 8), 20, def_->accent, 2);
    r.text(L"出口", cx, bottom + 6, 13, mix(def_->accent, rgb(255, 255, 255), 0.4f),
           TextAlign::Center);
    for (int i = 0; i < 5; ++i) {
        float a = time * 1.6f + i * 1.256f;
        float px = cx + std::cos(a) * 13;
        float py = top + 22 + std::sin(a * 1.2f) * 20;
        r.circle(px, py, 1.6f, mix(def_->accent, rgb(255, 255, 255), 0.5f));
    }
}

void Level::drawWorld(Renderer& r, float time) const {
    for (int rr = 0; rr < 18; ++rr) {
        for (int cc = 0; cc < 32; ++cc) {
            drawTile(r, cc, rr, time);
        }
    }
    if (hasDoor_) drawDoor(r, time);
    for (const Checkpoint& cp : checkpoints_) drawCheckpoint(r, cp, time);
    for (const Feather& f : feathers_) drawFeather(r, f, time);
    for (const Mover& m : movers_) drawMover(r, m, time);
    for (const Shooter& s : shooters_) drawShooter(r, s, time);
    for (const Faller& f : fallers_) drawFaller(r, f, time);
    for (const Dart& d : darts_) {
        if (!d.alive) continue;
        r.line(Vec2(d.pos.x - d.vel.x * 0.03f, d.pos.y), Vec2(d.pos.x, d.pos.y),
               mix(def_->accent, rgb(255, 255, 255), 0.45f), 3);
        r.fillRoundRect(RectF(d.pos.x - 7, d.pos.y - 2, 14, 4), 2, rgb(255, 245, 210));
    }
    r.fillRect(RectF(0, 0, 3, kScreenH), mix(def_->accent, rgb(0, 0, 0), 0.65f));
    r.fillRect(RectF(kScreenW - 3, 0, 3, kScreenH), mix(def_->accent, rgb(0, 0, 0), 0.65f));
}

// ============================================================
//  绘制：前景天气
// ============================================================
void Level::drawForeground(Renderer& r, float time) const {
    if (def_->theme == 0) {
        for (int i = 0; i < 30; ++i) {
            float x = std::fmod(hash01(i, 51, 3) * kScreenW + time * (8.0f + i * 0.7f), kScreenW);
            float y = 120 + hash01(i, 53, 3) * 540 + std::sin(time * 1.4f + i) * 12;
            float a = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(time * 2.0f + i * 1.2f));
            r.circle(x, y, 1.5f * a, mix(def_->accent, rgb(255, 255, 255), 0.3f));
        }
    } else if (def_->theme == 1) {
        for (int i = 0; i < 18; ++i) {
            float x = std::fmod(hash01(i, 57, 4) * kScreenW + time * (18.0f + i * 2.0f), kScreenW + 80) - 40;
            float y = 60 + hash01(i, 59, 4) * 600 + std::sin(time * 1.1f + i) * 22;
            float ang = time * 1.2f + i;
            Vec2 c(x, y);
            r.line(Vec2(c.x - std::cos(ang) * 5, c.y - std::sin(ang) * 3),
                   Vec2(c.x + std::cos(ang) * 5, c.y + std::sin(ang) * 3),
                   mix(rgb(255, 160, 110), rgb(160, 70, 60), 0.4f), 2);
        }
    } else if (def_->theme == 2) {
        for (int i = 0; i < 4; ++i) {
            float y = 250 + i * 110 + std::sin(time * 0.5f + i) * 12;
            r.alphaRect(RectF(0, y, kScreenW, 34), mix(def_->skyBottom, rgb(180, 255, 240), 0.4f), 8);
        }
        for (int i = 0; i < 20; ++i) {
            float x = hash01(i, 61, 6) * kScreenW;
            float y = std::fmod(time * (30.0f + i * 2.0f) + hash01(i, 63, 6) * 700, 760) - 20;
            r.line(Vec2(x, y), Vec2(x, y + 7), mix(def_->accent, rgb(255, 255, 255), 0.2f), 1);
        }
    } else if (def_->theme == 3) {
        for (int i = 0; i < 34; ++i) {
            float x = std::fmod(hash01(i, 67, 8) * kScreenW + time * (6.0f + i * 0.5f), kScreenW);
            float y = 60 + hash01(i, 69, 8) * 620 + std::sin(time * 1.2f + i) * 18;
            r.circle(x, y, 1.4f, mix(def_->accent, rgb(255, 255, 255), 0.4f));
        }
    } else if (def_->theme == 4) {
        for (int i = 0; i < 110; ++i) {
            float x = std::fmod(hash01(i, 71, 9) * kScreenW + time * (220.0f + i * 3.0f), kScreenW + 100) - 50;
            float y = std::fmod(hash01(i, 73, 9) * 760 + time * (620.0f + i * 8.0f), 760);
            r.line(Vec2(x, y), Vec2(x - 6, y + 22), mix(rgb(150, 200, 230), rgb(30, 60, 90), 0.45f), 1);
        }
        float flash = std::fmod(time, 5.3f);
        if (flash < 0.12f) {
            r.alphaRect(RectF(0, 0, kScreenW, kScreenH), rgb(210, 235, 255), 18);
        }
    } else {
        for (int i = 0; i < 46; ++i) {
            float x = std::fmod(hash01(i, 77, 5) * kScreenW + std::sin(time * 0.7f + i) * 40, kScreenW);
            float y = 720 - std::fmod(time * (30.0f + i * 2.5f) + hash01(i, 79, 5) * 720, 760);
            float s = 1.0f + hash01(i, 81, 5) * 2.0f;
            r.circle(x, y, s, mix(rgb(255, 120, 60), rgb(255, 220, 140), hash01(i, 83, 5)));
        }
    }
}

} // namespace jubi
