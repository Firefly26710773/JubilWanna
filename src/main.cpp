// ============================================================
//  main.cpp  -  Win32 窗口、输入、固定步长主循环与截图入口
#include "Game.h"

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <shellapi.h>

#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <string>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "shell32.lib")

namespace {

constexpr wchar_t kClassName[] = L"JubiIWannaWindow";
constexpr wchar_t kWindowTitle[] = L"啾比大冒险 · I WANNA";
constexpr int kClientW = 1280;
constexpr int kClientH = 720;

jubi::Game* g_game = nullptr;
HWND g_hwnd = nullptr;
bool g_keys[256]{};
bool g_pressed[256]{};
bool g_inputReset = false;
bool g_screenshotRequest = false;
bool g_fullscreen = false;
WINDOWPLACEMENT g_savedPlacement{sizeof(WINDOWPLACEMENT)};
LONG_PTR g_savedStyle = 0;
int g_clientW = kClientW;
int g_clientH = kClientH;

bool KeyDown(int vk) { return vk >= 0 && vk < 256 && g_keys[vk]; }
bool KeyPressed(int vk) { return vk >= 0 && vk < 256 && g_pressed[vk]; }

void ClearInput() {
    std::memset(g_keys, 0, sizeof(g_keys));
    std::memset(g_pressed, 0, sizeof(g_pressed));
}

bool MouseToGame(int mx, int my, int& gx, int& gy) {
    if (g_clientW < 1 || g_clientH < 1) return false;
    float scaleX = static_cast<float>(g_clientW) / static_cast<float>(jubi::kScreenW);
    float scaleY = static_cast<float>(g_clientH) / static_cast<float>(jubi::kScreenH);
    float scale = scaleX < scaleY ? scaleX : scaleY;
    if (scale <= 0.0f) return false;
    float dw = static_cast<float>(jubi::kScreenW) * scale;
    float dh = static_cast<float>(jubi::kScreenH) * scale;
    float offX = (static_cast<float>(g_clientW) - dw) * 0.5f;
    float offY = (static_cast<float>(g_clientH) - dh) * 0.5f;
    gx = static_cast<int>((static_cast<float>(mx) - offX) / scale);
    gy = static_cast<int>((static_cast<float>(my) - offY) / scale);
    return true;
}

bool MessageMouseClient(HWND hwnd, int& x, int& y) {
    DWORD pos = GetMessagePos();
    POINT pt{GET_X_LPARAM(pos), GET_Y_LPARAM(pos)};
    if (!ScreenToClient(hwnd, &pt)) return false;
    x = pt.x;
    y = pt.y;
    return true;
}

double NowSeconds() {
    static LARGE_INTEGER freq{};
    static bool ready = false;
    if (!ready) {
        QueryPerformanceFrequency(&freq);
        ready = true;
    }
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return static_cast<double>(c.QuadPart) / static_cast<double>(freq.QuadPart);
}

std::wstring ExeDirectory() {
    wchar_t buf[MAX_PATH]{};
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf, n);
    size_t p = s.find_last_of(L"\\/");
    return (p == std::wstring::npos) ? L"." : s.substr(0, p);
}

void ToggleFullscreen(HWND hwnd) {
    if (!g_fullscreen) {
        g_savedPlacement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hwnd, &g_savedPlacement);
        g_savedStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);

        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoW(monitor, &mi)) return;
        RECT r = mi.rcMonitor;

        SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_fullscreen = true;
    } else {
        SetWindowLongPtrW(hwnd, GWL_STYLE, g_savedStyle);
        SetWindowPlacement(hwnd, &g_savedPlacement);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_fullscreen = false;
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

std::wstring TimestampedShotPath() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t name[96];
    std::swprintf(name, 96, L"jubi_%04d%02d%02d_%02d%02d%02d_%03d.bmp",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                  st.wMilliseconds);
    std::filesystem::path dir = std::filesystem::path(ExeDirectory()) / L"shots";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return (dir / name).wstring();
}

bool SaveShot(jubi::Game& game, const std::wstring& path) {
    std::filesystem::path p(path);
    std::error_code ec;
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    game.render();
    if (!game.saveScreenshot(path)) return false;

    std::ofstream log(std::filesystem::path(ExeDirectory()) / L"shots" / L"last_shot.txt");
    if (log) log << std::filesystem::path(path).string() << "\n";
    return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            g_clientW = LOWORD(lParam);
            g_clientH = HIWORD(lParam);
            if (g_clientW < 1) g_clientW = 1;
            if (g_clientH < 1) g_clientH = 1;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = 960;
            mmi->ptMinTrackSize.y = 600;
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            if (g_game && g_clientW > 1 && g_clientH > 1) {
                g_game->render();
                g_game->present(dc, g_clientW, g_clientH);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            int vk = static_cast<int>(wParam);
            if (vk == VK_F11) {
                ToggleFullscreen(hwnd);
                return 0;
            }
            if (vk == VK_F12) {
                g_screenshotRequest = true;
                return 0;
            }
            if (vk >= 0 && vk < 256) {
                g_keys[vk] = true;
                if ((lParam & (1LL << 30)) == 0) g_pressed[vk] = true;
            }
            return 0;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP: {
            int vk = static_cast<int>(wParam);
            if (vk >= 0 && vk < 256) g_keys[vk] = false;
            return 0;
        }

        case WM_KILLFOCUS:
        case WM_ACTIVATEAPP:
            if (msg == WM_KILLFOCUS || wParam == FALSE) {
                ClearInput();
                g_inputReset = true;
                if (g_game) g_game->notifyFocusLost();
            }
            return 0;

        case WM_MOUSEMOVE: {
            if (g_game) {
                int cx = 0, cy = 0, gx = 0, gy = 0;
                if (MessageMouseClient(hwnd, cx, cy) && MouseToGame(cx, cy, gx, gy)) {
                    g_game->handleMouseMove(gx, gy);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            if (g_game) {
                int cx = 0, cy = 0, gx = 0, gy = 0;
                if (MessageMouseClient(hwnd, cx, cy) && MouseToGame(cx, cy, gx, gy)) {
                    g_game->handleMouseClick(gx, gy);
                }
            }
            return 0;
        }

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                if (g_game && g_game->wantsCursor()) {
                    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                } else {
                    SetCursor(nullptr);
                }
                return TRUE;
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            g_hwnd = nullptr;
            g_game = nullptr;
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

HWND CreateGameWindow(HINSTANCE instance, bool visible, int showCmd) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kClassName;
    wc.lpszMenuName = nullptr;
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc)) return nullptr;

    RECT rect{0, 0, kClientW, kClientH};
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int x = work.left + ((work.right - work.left) - w) / 2;
    int y = work.top + ((work.bottom - work.top) - h) / 2;
    if (x < work.left) x = work.left;
    if (y < work.top) y = work.top;

    HWND hwnd = CreateWindowExW(0, kClassName, kWindowTitle,
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                x, y, w, h, nullptr, nullptr, instance, nullptr);
    if (!hwnd) return nullptr;

    g_hwnd = hwnd;
    if (visible) {
        ShowWindow(hwnd, showCmd);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

jubi::InputState ReadHeldInput() {
    jubi::InputState in{};
    in.left = KeyDown('A') || KeyDown(VK_LEFT);
    in.right = KeyDown('D') || KeyDown(VK_RIGHT);
    in.jumpHeld = KeyDown(VK_SPACE) || KeyDown('W') || KeyDown(VK_UP) || KeyDown('K');
    return in;
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    timeBeginPeriod(1);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring shotPath;
    std::wstring state;
    std::wstring testName;
    std::wstring testOut = L"shots\\playtest.bmp";
    int room = -1;
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring a = argv[i];
            if (a == L"--shot" && i + 1 < argc) {
                shotPath = argv[++i];
            } else if (a == L"--room" && i + 1 < argc) {
                try {
                    room = std::stoi(argv[++i]);
                } catch (...) {
                    room = -1;
                }
            } else if (a == L"--state" && i + 1 < argc) {
                state = argv[++i];
            } else if (a == L"--test" && i + 1 < argc) {
                testName = argv[++i];
            } else if (a == L"--out" && i + 1 < argc) {
                testOut = argv[++i];
            }
        }
        LocalFree(argv);
    }

    HWND hwnd = CreateGameWindow(instance, shotPath.empty() && testName.empty(),
                                 (nCmdShow == SW_SHOWDEFAULT) ? SW_SHOWNORMAL : nCmdShow);
    if (!hwnd) {
        MessageBoxW(nullptr, L"窗口创建失败。", L"啾比大冒险", MB_ICONERROR);
        timeEndPeriod(1);
        return 1;
    }

    jubi::Game game;
    if (!game.init(hwnd)) {
        MessageBoxW(nullptr, L"渲染器初始化失败。", L"啾比大冒险", MB_ICONERROR);
        DestroyWindow(hwnd);
        timeEndPeriod(1);
        return 1;
    }
    g_game = &game;

    if (!testName.empty()) {
        int rc = game.runAutomatedTest(room < 0 ? 0 : room, testName, testOut) ? 0 : 2;
        game.shutdown();
        g_game = nullptr;
        DestroyWindow(hwnd);
        UnregisterClassW(kClassName, instance);
        timeEndPeriod(1);
        return rc;
    }

    if (!shotPath.empty()) {
        game.prepareScreenshot(room, state);
        int rc = SaveShot(game, shotPath) ? 0 : 2;
        game.shutdown();
        g_game = nullptr;
        DestroyWindow(hwnd);
        UnregisterClassW(kClassName, instance);
        timeEndPeriod(1);
        return rc;
    }

    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    ClearInput();

    bool running = true;
    bool latchJump = false;
    bool latchUp = false;
    bool latchDown = false;
    bool latchConfirm = false;
    bool latchBack = false;
    bool latchRestart = false;
    bool latchMute = false;
    bool latchDebug = false;

    const double step = 1.0 / 120.0;
    double accumulator = 0.0;
    double previous = NowSeconds();

    while (running) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        if (g_inputReset) {
            g_inputReset = false;
            latchJump = latchUp = latchDown = latchConfirm = false;
            latchBack = latchRestart = latchMute = latchDebug = false;
        }

        double now = NowSeconds();
        double frame = now - previous;
        previous = now;
        if (frame > 0.25) frame = 0.25;
        if (frame < 0.0) frame = 0.0;
        accumulator += frame;

        latchJump = latchJump || KeyPressed(VK_SPACE) || KeyPressed('W') ||
                    KeyPressed(VK_UP) || KeyPressed('K');
        latchUp = latchUp || KeyPressed(VK_UP) || KeyPressed('W');
        latchDown = latchDown || KeyPressed(VK_DOWN) || KeyPressed('S');
        latchConfirm = latchConfirm || KeyPressed(VK_RETURN) || KeyPressed(VK_SPACE);
        latchBack = latchBack || KeyPressed(VK_ESCAPE);
        latchRestart = latchRestart || KeyPressed('R');
        latchMute = latchMute || KeyPressed('M');
        latchDebug = latchDebug || KeyPressed(VK_F3);
        std::memset(g_pressed, 0, sizeof(g_pressed));

        if (IsIconic(hwnd)) {
            Sleep(16);
            accumulator = 0.0;
            continue;
        }

        int steps = 0;
        while (accumulator >= step && steps < 12) {
            jubi::InputState in = ReadHeldInput();
            if (steps == 0) {
                in.jumpPressed = latchJump;
                in.upPressed = latchUp;
                in.downPressed = latchDown;
                in.confirmPressed = latchConfirm;
                in.backPressed = latchBack;
                in.restartPressed = latchRestart;
                in.mutePressed = latchMute;
                in.debugPressed = latchDebug;
                latchJump = latchUp = latchDown = latchConfirm = false;
                latchBack = latchRestart = latchMute = latchDebug = false;
            }
            game.update(static_cast<float>(step), in);
            accumulator -= step;
            ++steps;
            if (game.quitRequested()) {
                running = false;
                break;
            }
        }
        if (steps >= 12) accumulator = 0.0;
        if (!running) break;

        game.render();
        HDC dc = GetDC(hwnd);
        if (dc) {
            game.present(dc, g_clientW, g_clientH);
            ReleaseDC(hwnd, dc);
        }

        if (g_screenshotRequest) {
            g_screenshotRequest = false;
            SaveShot(game, TimestampedShotPath());
        }

        double elapsed = NowSeconds() - now;
        double target = 1.0 / 60.0;
        if (elapsed < target) {
            DWORD ms = static_cast<DWORD>((target - elapsed) * 1000.0);
            if (ms > 0) Sleep(ms);
        }
    }

    game.shutdown();
    g_game = nullptr;
    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    UnregisterClassW(kClassName, instance);
    timeEndPeriod(1);
    return 0;
}

