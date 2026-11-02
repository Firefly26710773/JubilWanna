// ============================================================
//  Renderer.h  -  GDI 双缓冲渲染器
//  所有绘制先写入 1280x720 的后备缓冲区，再等比缩放呈现到窗口
// ============================================================
#pragma once

#include "Common.h"

#include <map>
#include <string>

namespace jubi {

enum class TextAlign { Left, Center, Right };

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(HWND hwnd);
    void shutdown();

    HDC dc() const { return memDC_; }
    int width() const { return kScreenW; }
    int height() const { return kScreenH; }

    void clear(Color c);
    void present(HDC target, int clientW, int clientH);

    // ---- 基础图形 ----
    void fillRect(const RectF& r, Color c);
    void fillRoundRect(const RectF& r, float radius, Color c);
    void strokeRect(const RectF& r, Color c, int width = 2);
    void strokeRoundRect(const RectF& r, float radius, Color c, int width = 2);
    void line(Vec2 a, Vec2 b, Color c, int width = 2);
    void circle(float cx, float cy, float radius, Color c);
    void circleOutline(float cx, float cy, float radius, Color c, int width = 2);
    void ellipse(const RectF& r, Color c);
    void triangle(Vec2 a, Vec2 b, Vec2 c, Color color);
    void polygon(const Vec2* pts, int count, Color c);
    void arc(float cx, float cy, float radius, float startRad, float endRad, Color c, int width = 2);

    // ---- 半透明 ----
    void alphaRect(const RectF& r, Color c, int alpha);
    void vignette(int strength = 150);

    // ---- 文字 ----
    void text(const std::wstring& s, float x, float y, int size, Color c,
              TextAlign align = TextAlign::Left, bool bold = false,
              const wchar_t* family = L"Microsoft YaHei UI");
    void textShadow(const std::wstring& s, float x, float y, int size, Color c,
                    Color shadow, float offset = 2.0f,
                    TextAlign align = TextAlign::Left, bool bold = false,
                    const wchar_t* family = L"Microsoft YaHei UI");
    void textBlock(const std::wstring& s, const RectF& box, int size, Color c,
                   TextAlign align = TextAlign::Left, bool bold = false,
                   const wchar_t* family = L"Microsoft YaHei UI");
    int  measureText(const std::wstring& s, int size, bool bold = false,
                     const wchar_t* family = L"Microsoft YaHei UI");
    int  measureTextHeight(const std::wstring& s, int width, int size, bool bold = false,
                           const wchar_t* family = L"Microsoft YaHei UI");

    // ---- 视口偏移（屏幕震动） ----
    void setOffset(int x, int y);
    void resetOffset();

    // ---- 工具 ----
    void saveBMP(const std::wstring& path) const;

private:
    HFONT fontFor(int size, bool bold, const wchar_t* family);

    HWND hwnd_ = nullptr;
    HDC memDC_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;

    HDC alphaDC_ = nullptr;
    HBITMAP alphaBitmap_ = nullptr;
    HGDIOBJ oldAlphaBitmap_ = nullptr;
    uint32_t* alphaBits_ = nullptr;

    HDC vignetteDC_ = nullptr;
    HBITMAP vignetteBitmap_ = nullptr;
    HGDIOBJ oldVignetteBitmap_ = nullptr;

    std::map<std::wstring, HFONT> fonts_;
};

} // namespace jubi
