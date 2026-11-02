// ============================================================
//  Renderer.cpp  -  GDI 双缓冲渲染器实现
// ============================================================
#include "Renderer.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace jubi {

namespace {

struct BrushGuard {
    HDC dc;
    HBRUSH brush;
    HGDIOBJ old;
    BrushGuard(HDC d, Color c) : dc(d), brush(CreateSolidBrush(toREF(c))) {
        old = SelectObject(dc, brush);
    }
    ~BrushGuard() {
        SelectObject(dc, old);
        DeleteObject(brush);
    }
};

struct PenGuard {
    HDC dc;
    HPEN pen;
    HGDIOBJ old;
    PenGuard(HDC d, Color c, int width) : dc(d) {
        pen = CreatePen(PS_SOLID, width < 1 ? 1 : width, toREF(c));
        old = SelectObject(dc, pen);
    }
    ~PenGuard() {
        SelectObject(dc, old);
        DeleteObject(pen);
    }
};

RECT toRect(const RectF& r) {
    RECT rc;
    rc.left = iround(r.x);
    rc.top = iround(r.y);
    rc.right = iround(r.x + r.w);
    rc.bottom = iround(r.y + r.h);
    if (rc.right < rc.left) std::swap(rc.right, rc.left);
    if (rc.bottom < rc.top) std::swap(rc.bottom, rc.top);
    return rc;
}

} // namespace

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(HWND hwnd) {
    hwnd_ = hwnd;
    HDC windowDC = GetDC(hwnd);
    if (!windowDC) return false;

    memDC_ = CreateCompatibleDC(windowDC);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kScreenW;
    bmi.bmiHeader.biHeight = -kScreenH; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    bitmap_ = CreateDIBSection(windowDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap_) {
        ReleaseDC(hwnd, windowDC);
        return false;
    }
    oldBitmap_ = SelectObject(memDC_, bitmap_);

    // 用于 AlphaBlend 的 1x1 纯色源
    alphaDC_ = CreateCompatibleDC(windowDC);
    BITMAPINFO abi{};
    abi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    abi.bmiHeader.biWidth = 1;
    abi.bmiHeader.biHeight = -1;
    abi.bmiHeader.biPlanes = 1;
    abi.bmiHeader.biBitCount = 32;
    abi.bmiHeader.biCompression = BI_RGB;
    alphaBitmap_ = CreateDIBSection(windowDC, &abi, DIB_RGB_COLORS,
                                    reinterpret_cast<void**>(&alphaBits_), nullptr, 0);
    if (alphaBitmap_) oldAlphaBitmap_ = SelectObject(alphaDC_, alphaBitmap_);

    // 暗角图层：每像素 alpha，中心透明，四周渐黑
    vignetteDC_ = CreateCompatibleDC(windowDC);
    BITMAPINFO vbi{};
    vbi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    vbi.bmiHeader.biWidth = kScreenW;
    vbi.bmiHeader.biHeight = -kScreenH;
    vbi.bmiHeader.biPlanes = 1;
    vbi.bmiHeader.biBitCount = 32;
    vbi.bmiHeader.biCompression = BI_RGB;
    void* vbits = nullptr;
    vignetteBitmap_ = CreateDIBSection(windowDC, &vbi, DIB_RGB_COLORS, &vbits, nullptr, 0);
    if (vignetteBitmap_) {
        oldVignetteBitmap_ = SelectObject(vignetteDC_, vignetteBitmap_);
        uint32_t* px = static_cast<uint32_t*>(vbits);
        for (int y = 0; y < kScreenH; ++y) {
            float ny = (y - kScreenH * 0.5f) / (kScreenH * 0.5f);
            for (int x = 0; x < kScreenW; ++x) {
                float nx = (x - kScreenW * 0.5f) / (kScreenW * 0.5f);
                float d = std::sqrt(nx * nx * 0.80f + ny * ny);
                float t = clampf((d - 0.45f) / 0.75f, 0.0f, 1.0f);
                t = t * t * (3.0f - 2.0f * t);
                uint32_t a = static_cast<uint32_t>(t * 205.0f + 0.5f);
                px[y * kScreenW + x] = (a << 24);
            }
        }
    }

    SetBkMode(memDC_, TRANSPARENT);
    SetStretchBltMode(memDC_, COLORONCOLOR);
    ReleaseDC(hwnd, windowDC);
    return true;
}

void Renderer::shutdown() {
    for (auto& kv : fonts_) DeleteObject(kv.second);
    fonts_.clear();

    if (alphaDC_) {
        if (oldAlphaBitmap_) SelectObject(alphaDC_, oldAlphaBitmap_);
        DeleteDC(alphaDC_);
        alphaDC_ = nullptr;
    }
    if (alphaBitmap_) {
        DeleteObject(alphaBitmap_);
        alphaBitmap_ = nullptr;
    }
    if (vignetteDC_) {
        if (oldVignetteBitmap_) SelectObject(vignetteDC_, oldVignetteBitmap_);
        DeleteDC(vignetteDC_);
        vignetteDC_ = nullptr;
    }
    if (vignetteBitmap_) {
        DeleteObject(vignetteBitmap_);
        vignetteBitmap_ = nullptr;
    }
    if (memDC_) {
        if (oldBitmap_) SelectObject(memDC_, oldBitmap_);
        DeleteDC(memDC_);
        memDC_ = nullptr;
    }
    if (bitmap_) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    hwnd_ = nullptr;
}

void Renderer::clear(Color c) {
    HBRUSH br = CreateSolidBrush(toREF(c));
    RECT rc{0, 0, kScreenW, kScreenH};
    FillRect(memDC_, &rc, br);
    DeleteObject(br);
}

void Renderer::present(HDC target, int clientW, int clientH) {
    if (!target || !memDC_) return;
    if (clientW <= 0 || clientH <= 0) return;

    float sx = static_cast<float>(clientW) / kScreenW;
    float sy = static_cast<float>(clientH) / kScreenH;
    float s = std::min(sx, sy);
    int dw = iround(kScreenW * s);
    int dh = iround(kScreenH * s);
    int dx = (clientW - dw) / 2;
    int dy = (clientH - dh) / 2;

    // Only paint the letterbox bars. Filling the whole target black before
    // every BitBlt is what made the window flash at 60 fps.
    if (dx > 0 || dy > 0 || dx + dw < clientW || dy + dh < clientH) {
        HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
        if (dy > 0) {
            RECT r{0, 0, clientW, dy};
            FillRect(target, &r, black);
        }
        if (dy + dh < clientH) {
            RECT r{0, dy + dh, clientW, clientH};
            FillRect(target, &r, black);
        }
        if (dx > 0) {
            RECT r{0, dy, dx, dy + dh};
            FillRect(target, &r, black);
        }
        if (dx + dw < clientW) {
            RECT r{dx + dw, dy, clientW, dy + dh};
            FillRect(target, &r, black);
        }
        DeleteObject(black);
    }

    if (dw == kScreenW && dh == kScreenH) {
        BitBlt(target, dx, dy, dw, dh, memDC_, 0, 0, SRCCOPY);
    } else {
        SetStretchBltMode(target, HALFTONE);
        SetBrushOrgEx(target, 0, 0, nullptr);
        StretchBlt(target, dx, dy, dw, dh, memDC_, 0, 0, kScreenW, kScreenH, SRCCOPY);
        SetStretchBltMode(target, COLORONCOLOR);
    }
}

void Renderer::fillRect(const RectF& r, Color c) {
    HBRUSH br = CreateSolidBrush(toREF(c));
    RECT rc = toRect(r);
    FillRect(memDC_, &rc, br);
    DeleteObject(br);
}

void Renderer::fillRoundRect(const RectF& r, float radius, Color c) {
    BrushGuard br(memDC_, c);
    HGDIOBJ oldPen = SelectObject(memDC_, GetStockObject(NULL_PEN));
    RECT rc = toRect(r);
    RoundRect(memDC_, rc.left, rc.top, rc.right, rc.bottom,
              iround(radius * 2), iround(radius * 2));
    SelectObject(memDC_, oldPen);
}

void Renderer::strokeRect(const RectF& r, Color c, int width) {
    HGDIOBJ oldBrush = SelectObject(memDC_, GetStockObject(NULL_BRUSH));
    PenGuard pen(memDC_, c, width);
    RECT rc = toRect(r);
    int inset = width / 2;
    Rectangle(memDC_, rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset);
    SelectObject(memDC_, oldBrush);
}

void Renderer::strokeRoundRect(const RectF& r, float radius, Color c, int width) {
    HGDIOBJ oldBrush = SelectObject(memDC_, GetStockObject(NULL_BRUSH));
    PenGuard pen(memDC_, c, width);
    RECT rc = toRect(r);
    int inset = width / 2;
    RoundRect(memDC_, rc.left + inset, rc.top + inset, rc.right - inset, rc.bottom - inset,
              iround(radius * 2), iround(radius * 2));
    SelectObject(memDC_, oldBrush);
}

void Renderer::line(Vec2 a, Vec2 b, Color c, int width) {
    PenGuard pen(memDC_, c, width);
    MoveToEx(memDC_, iround(a.x), iround(a.y), nullptr);
    LineTo(memDC_, iround(b.x), iround(b.y));
}

void Renderer::circle(float cx, float cy, float radius, Color c) {
    BrushGuard br(memDC_, c);
    HGDIOBJ oldPen = SelectObject(memDC_, GetStockObject(NULL_PEN));
    Ellipse(memDC_, iround(cx - radius), iround(cy - radius),
            iround(cx + radius), iround(cy + radius));
    SelectObject(memDC_, oldPen);
}

void Renderer::circleOutline(float cx, float cy, float radius, Color c, int width) {
    HGDIOBJ oldBrush = SelectObject(memDC_, GetStockObject(NULL_BRUSH));
    PenGuard pen(memDC_, c, width);
    Ellipse(memDC_, iround(cx - radius), iround(cy - radius),
            iround(cx + radius), iround(cy + radius));
    SelectObject(memDC_, oldBrush);
}

void Renderer::ellipse(const RectF& r, Color c) {
    BrushGuard br(memDC_, c);
    HGDIOBJ oldPen = SelectObject(memDC_, GetStockObject(NULL_PEN));
    RECT rc = toRect(r);
    Ellipse(memDC_, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(memDC_, oldPen);
}

void Renderer::triangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
    Vec2 pts[3] = {a, b, c};
    polygon(pts, 3, color);
}

void Renderer::polygon(const Vec2* pts, int count, Color c) {
    if (!pts || count < 3) return;
    std::vector<POINT> p(count);
    for (int i = 0; i < count; ++i) {
        p[i].x = iround(pts[i].x);
        p[i].y = iround(pts[i].y);
    }
    BrushGuard br(memDC_, c);
    HGDIOBJ oldPen = SelectObject(memDC_, GetStockObject(NULL_PEN));
    Polygon(memDC_, p.data(), count);
    SelectObject(memDC_, oldPen);
}

void Renderer::arc(float cx, float cy, float radius, float startRad, float endRad,
                   Color c, int width) {
    PenGuard pen(memDC_, c, width);
    HGDIOBJ oldBrush = SelectObject(memDC_, GetStockObject(NULL_BRUSH));
    int steps = 40;
    bool started = false;
    for (int i = 0; i <= steps; ++i) {
        float t = startRad + (endRad - startRad) * (static_cast<float>(i) / steps);
        int x = iround(cx + std::cos(t) * radius);
        int y = iround(cy + std::sin(t) * radius);
        if (!started) {
            MoveToEx(memDC_, x, y, nullptr);
            started = true;
        } else {
            LineTo(memDC_, x, y);
        }
    }
    SelectObject(memDC_, oldBrush);
}

void Renderer::alphaRect(const RectF& r, Color c, int alpha) {
    if (!alphaDC_ || !alphaBitmap_ || alpha <= 0) return;
    alpha = (std::min)(alpha, 255);
    alphaBits_[0] = (static_cast<uint32_t>(c.r) << 16) |
                    (static_cast<uint32_t>(c.g) << 8) |
                    static_cast<uint32_t>(c.b);

    RECT rc = toRect(r);
    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = static_cast<BYTE>(alpha);
    bf.AlphaFormat = 0;
    AlphaBlend(memDC_, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
               alphaDC_, 0, 0, 1, 1, bf);
}

void Renderer::vignette(int strength) {
    if (!vignetteDC_ || !vignetteBitmap_) return;
    strength = (std::max)(0, (std::min)(strength, 255));
    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = static_cast<BYTE>(strength);
    bf.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(memDC_, 0, 0, kScreenW, kScreenH, vignetteDC_, 0, 0, kScreenW, kScreenH, bf);
}

HFONT Renderer::fontFor(int size, bool bold, const wchar_t* family) {
    if (size < 6) size = 6;
    std::wstring key = std::to_wstring(size) + L"|" + (bold ? L"1" : L"0") + L"|" + family;
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;
    HFONT f = CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family);
    fonts_[key] = f;
    return f;
}

void Renderer::text(const std::wstring& s, float x, float y, int size, Color c,
                    TextAlign align, bool bold, const wchar_t* family) {
    if (s.empty()) return;
    HFONT f = fontFor(size, bold, family);
    HGDIOBJ oldFont = SelectObject(memDC_, f);
    SetTextColor(memDC_, toREF(c));
    SetBkMode(memDC_, TRANSPARENT);

    SIZE sz{};
    GetTextExtentPoint32W(memDC_, s.c_str(), static_cast<int>(s.size()), &sz);
    int px = iround(x);
    if (align == TextAlign::Center) px -= sz.cx / 2;
    if (align == TextAlign::Right) px -= sz.cx;
    TextOutW(memDC_, px, iround(y), s.c_str(), static_cast<int>(s.size()));
    SelectObject(memDC_, oldFont);
}

void Renderer::textShadow(const std::wstring& s, float x, float y, int size, Color c,
                          Color shadow, float offset, TextAlign align, bool bold,
                          const wchar_t* family) {
    text(s, x + offset, y + offset, size, shadow, align, bold, family);
    text(s, x, y, size, c, align, bold, family);
}

void Renderer::textBlock(const std::wstring& s, const RectF& box, int size, Color c,
                         TextAlign align, bool bold, const wchar_t* family) {
    HFONT f = fontFor(size, bold, family);
    HGDIOBJ oldFont = SelectObject(memDC_, f);
    SetTextColor(memDC_, toREF(c));
    SetBkMode(memDC_, TRANSPARENT);

    RECT rc = toRect(box);
    UINT flags = DT_TOP | DT_WORDBREAK | DT_NOPREFIX;
    if (align == TextAlign::Center) flags |= DT_CENTER;
    if (align == TextAlign::Right) flags |= DT_RIGHT;
    DrawTextW(memDC_, s.c_str(), static_cast<int>(s.size()), &rc, flags);
    SelectObject(memDC_, oldFont);
}

int Renderer::measureText(const std::wstring& s, int size, bool bold, const wchar_t* family) {
    if (s.empty()) return 0;
    HFONT f = fontFor(size, bold, family);
    HGDIOBJ oldFont = SelectObject(memDC_, f);
    SIZE sz{};
    GetTextExtentPoint32W(memDC_, s.c_str(), static_cast<int>(s.size()), &sz);
    SelectObject(memDC_, oldFont);
    return sz.cx;
}

int Renderer::measureTextHeight(const std::wstring& s, int width, int size, bool bold,
                                const wchar_t* family) {
    HFONT f = fontFor(size, bold, family);
    HGDIOBJ oldFont = SelectObject(memDC_, f);
    RECT rc{0, 0, width, 0};
    DrawTextW(memDC_, s.c_str(), static_cast<int>(s.size()), &rc,
              DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(memDC_, oldFont);
    return rc.bottom - rc.top;
}

void Renderer::setOffset(int x, int y) { SetViewportOrgEx(memDC_, x, y, nullptr); }
void Renderer::resetOffset() { SetViewportOrgEx(memDC_, 0, 0, nullptr); }

void Renderer::saveBMP(const std::wstring& path) const {
    if (!memDC_) return;
    GdiFlush();

    const int w = kScreenW;
    const int h = kScreenH;
    const int stride = w * 4;
    const int imgSize = stride * h;
    const int fileSize = 54 + imgSize;

    std::vector<BYTE> buf(static_cast<size_t>(fileSize), 0);
    buf[0] = 'B';
    buf[1] = 'M';
    auto put32 = [&](int offset, uint32_t v) {
        buf[offset + 0] = static_cast<BYTE>(v & 0xFF);
        buf[offset + 1] = static_cast<BYTE>((v >> 8) & 0xFF);
        buf[offset + 2] = static_cast<BYTE>((v >> 16) & 0xFF);
        buf[offset + 3] = static_cast<BYTE>((v >> 24) & 0xFF);
    };
    put32(2, static_cast<uint32_t>(fileSize));
    put32(10, 54);
    put32(14, 40);
    put32(18, static_cast<uint32_t>(w));
    put32(22, static_cast<uint32_t>(h));
    buf[26] = 1;
    buf[28] = 32;
    put32(34, static_cast<uint32_t>(imgSize));

    // 从 DIB 直接读：32bpp top-down，每行 w*4，无 padding
    BITMAP bm{};
    if (GetObjectW(bitmap_, sizeof(bm), &bm) && bm.bmBits) {
        const BYTE* src = static_cast<const BYTE*>(bm.bmBits);
        for (int y = 0; y < h; ++y) {
            const BYTE* srcRow = src + static_cast<size_t>(y) * bm.bmWidthBytes;
            BYTE* dstRow = buf.data() + 54 + static_cast<size_t>(h - 1 - y) * stride;
            memcpy(dstRow, srcRow, stride);
        }
    }

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, buf.data(), static_cast<DWORD>(buf.size()), &written, nullptr);
    CloseHandle(file);
}

} // namespace jubi
