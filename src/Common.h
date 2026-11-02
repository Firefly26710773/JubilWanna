// ============================================================
//  Common.h  -  公共类型与数学工具
//  I WANNA: 啾比大冒险  |  C++ 课程设计
// ============================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>

namespace jubi {

// ---------- 逻辑分辨率 ----------
constexpr int   kScreenW = 1280;
constexpr int   kScreenH = 720;
constexpr int   kTile     = 40;
constexpr float kPi       = 3.14159265358979323846f;

constexpr float kPlayerW  = 26.0f;
constexpr float kPlayerH  = 38.0f;

// ---------- 基础数学 ----------
inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}
inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}
inline float approach(float cur, float target, float maxDelta) {
    if (cur < target) return std::min(cur + maxDelta, target);
    return std::max(cur - maxDelta, target);
}
inline int iround(float v) {
    return static_cast<int>(std::lround(v));
}
inline float smoothstep(float t) {
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline float pingpong01(float t) {
    // 0 -> 1 -> 0 的平滑往返
    return 0.5f - 0.5f * std::cos(t * 2.0f * kPi);
}
// 稳定的二维哈希，用于程序化纹理，保证每帧一致
inline float hash01(int x, int y, int seed = 0) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u
               + static_cast<uint32_t>(y) * 668265263u
               + static_cast<uint32_t>(seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

// ---------- 向量 ----------
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    float length() const { return std::sqrt(x * x + y * y); }
    Vec2 normalized() const {
        float l = length();
        return l > 0.0001f ? Vec2{x / l, y / l} : Vec2{0.0f, 0.0f};
    }
};

// ---------- 矩形（左上角 + 宽高） ----------
struct RectF {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    RectF() = default;
    RectF(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}

    float left() const { return x; }
    float top() const { return y; }
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }

    bool intersects(const RectF& o) const {
        return x < o.right() && right() > o.x && y < o.bottom() && bottom() > o.y;
    }
    bool contains(const Vec2& p) const {
        return p.x >= x && p.x <= right() && p.y >= y && p.y <= bottom();
    }
    RectF inflated(float dx, float dy) const {
        return {x - dx, y - dy, w + dx * 2.0f, h + dy * 2.0f};
    }
    RectF offset(float dx, float dy) const {
        return {x + dx, y + dy, w, h};
    }
    static RectF fromLTRB(float l, float t, float r, float b) {
        return {l, t, r - l, b - t};
    }
};

// ---------- 颜色 ----------
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

constexpr Color rgb(int r, int g, int b) {
    return Color{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
}

inline COLORREF toREF(Color c) {
    return RGB(c.r, c.g, c.b);
}
inline Color mix(Color a, Color b, float t) {
    t = clampf(t, 0.0f, 1.0f);
    return Color{
        static_cast<uint8_t>(a.r + (b.r - a.r) * t),
        static_cast<uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<uint8_t>(a.b + (b.b - a.b) * t)
    };
}
inline Color brighten(Color c, float amount) {
    return mix(c, rgb(255, 255, 255), amount);
}
inline Color darken(Color c, float amount) {
    return mix(c, rgb(0, 0, 0), amount);
}

// ---------- 输入快照 ----------
struct InputState {
    bool left = false;
    bool right = false;
    bool jumpPressed = false;
    bool jumpHeld = false;
    bool upPressed = false;
    bool downPressed = false;
    bool confirmPressed = false;
    bool backPressed = false;
    bool restartPressed = false;
    bool mutePressed = false;
    bool debugPressed = false;
};

// ---------- 时间格式 mm:ss.t ----------
inline std::wstring formatTime(float seconds) {
    if (seconds < 0.0f) seconds = 0.0f;
    int totalTenths = static_cast<int>(seconds * 10.0f + 0.5f);
    int mins = totalTenths / 600;
    int secs = (totalTenths / 10) % 60;
    int tenths = totalTenths % 10;
    wchar_t buf[32];
    std::swprintf(buf, 32, L"%02d:%02d.%d", mins, secs, tenths);
    return buf;
}

inline std::wstring padInt(int value, int width) {
    std::wstring s = std::to_wstring(value);
    while (static_cast<int>(s.size()) < width) s = L"0" + s;
    return s;
}

} // namespace jubi
