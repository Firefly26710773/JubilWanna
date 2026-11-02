// ============================================================
//  Level.h  -  关卡数据、瓦片碰撞、动态机关与场景绘制
// ============================================================
#pragma once

#include "Common.h"

#include <array>
#include <string>
#include <vector>

namespace jubi {

class Renderer;
class Audio;

enum class TileType : uint8_t {
    Empty,
    Solid,
    Platform,   // '=' 单向平台
    Crumble,    // 'C' 踩下后碎裂
    Timed,      // 'T' 周期性出现
    Spike,      // '^' 尖刺
    Bounce      // 'B' 弹簧
};

struct Tile {
    TileType type = TileType::Empty;
    float timer = 0.0f;
    bool active = true;
    bool triggered = false;
};

struct MoverDef {
    float x0, y0, x1, y1;
    float w, h;
    float speed;
    float phase;
};

struct Mover {
    Vec2 a, b;
    Vec2 pos;
    Vec2 prev;
    float w = 80.0f;
    float h = 14.0f;
    float speed = 0.4f;
    float t = 0.0f;
    float deltaX = 0.0f;
    float deltaY = 0.0f;
};

struct ShooterDef {
    float x, y;
    float dir;
};

struct Shooter {
    Vec2 pos;
    float dir = -1.0f;
    float timer = 1.2f;
    float charge = 0.0f;
    bool charging = false;
};

struct Dart {
    Vec2 pos;
    Vec2 vel;
    bool alive = true;
};

struct FallerDef {
    float x, y;
};

struct Faller {
    Vec2 home;
    Vec2 pos;
    float vy = 0.0f;
    float warn = 0.0f;
    float rest = 0.0f;
    bool falling = false;
    bool settled = false;
};

struct Checkpoint {
    Vec2 tile;        // 瓦片坐标（列、行）
    Vec2 spawn;       // 复活点（玩家左上角）
    bool active = false;
};

struct Feather {
    Vec2 center;
    int col = 0;
    int row = 0;
    bool taken = false;
};

struct RoomDef {
    const wchar_t* name;
    const wchar_t* subtitle;
    const wchar_t* quote;
    Color skyTop;
    Color skyBottom;
    Color accent;
    Color tileDark;
    Color tileLight;
    std::array<const char*, 18> rows;
    std::vector<MoverDef> movers;
    std::vector<ShooterDef> shooters;
    std::vector<FallerDef> fallers;
    int theme = 0;
};

struct MoveYResult {
    bool landed = false;
    bool bounce = false;
    bool onMover = false;
    int moverIndex = -1;
};

class Level {
public:
    static int roomCount() { return 6; }

    bool load(int index);
    void resetRuntime();
    void update(float dt, const RectF& playerBox, bool playerGrounded, Audio& audio);

    void drawBackdrop(Renderer& r, float time) const;
    void drawWorld(Renderer& r, float time) const;
    void drawForeground(Renderer& r, float time) const;

    int roomIndex() const { return roomIndex_; }
    const wchar_t* name() const { return def_->name; }
    const wchar_t* subtitle() const { return def_->subtitle; }
    const wchar_t* quote() const { return def_->quote; }
    Color accent() const { return def_->accent; }
    int theme() const { return def_->theme; }

    Vec2 spawnPoint() const { return spawn_; }
    Vec2 respawnPoint() const;
    int activeCheckpoint() const { return activeCheckpoint_; }
    bool hasCheckpoint() const { return activeCheckpoint_ >= 0; }
    void setCheckpoint(int index);
    const std::vector<Checkpoint>& checkpoints() const { return checkpoints_; }
    const std::vector<Feather>& feathers() const { return feathers_; }
    bool featherTaken(int index) const;
    void setFeatherMask(int mask);
    int featherMask() const;
    int featherCount() const;

    bool solidAt(int c, int r) const;
    bool moveX(RectF& box, float dx) const;
    MoveYResult moveY(RectF& box, float dy) const;
    bool touchingBounce(const RectF& box) const;
    bool hazardAt(const RectF& box) const;
    bool goalAt(const RectF& box) const;
    int  collectFeather(const RectF& box);
    int  touchCheckpoint(const RectF& box);
    void triggerCrumbleUnder(const RectF& box);

    Vec2 moverDelta(int index) const;
    RectF moverRect(int index) const;

    float time() const { return time_; }

private:
    void parseMap();
    void drawBackdropTheme(Renderer& r, float time) const;
    void drawTile(Renderer& rnd, int c, int row, float time) const;
    void drawMover(Renderer& r, const Mover& m, float time) const;
    void drawShooter(Renderer& r, const Shooter& s, float time) const;
    void drawFaller(Renderer& r, const Faller& f, float time) const;
    void drawCheckpoint(Renderer& r, const Checkpoint& cp, float time) const;
    void drawFeather(Renderer& r, const Feather& f, float time) const;
    void drawDoor(Renderer& r, float time) const;
    float timedPhase(int c, int r) const;
    bool  timedActive(int c, int r) const;

    const RoomDef* def_ = nullptr;
    int roomIndex_ = 0;
    std::array<Tile, 32 * 18> tiles_{};
    Vec2 spawn_{};
    RectF door_{};
    bool hasDoor_ = false;
    std::vector<Mover> movers_;
    std::vector<Shooter> shooters_;
    std::vector<Dart> darts_;
    std::vector<Faller> fallers_;
    std::vector<Checkpoint> checkpoints_;
    std::vector<Feather> feathers_;
    int activeCheckpoint_ = -1;
    float time_ = 0.0f;
    float shootFlash_ = 0.0f;
};

} // namespace jubi
