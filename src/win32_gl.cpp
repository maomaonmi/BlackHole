#include "win32_gl.h"
#include "gui_config.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <dwmapi.h>
#include <wtsapi32.h>
#include <cstdio>
#include <cstring>

// MinGW may not define OCR_NORMAL
#ifndef OCR_NORMAL
#define OCR_NORMAL 32512
#endif

// ---- WGL extension declarations ----
typedef HGLRC (WINAPI* PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);
typedef BOOL  (WINAPI* PFN_wglSwapIntervalEXT)(int);
typedef const char* (WINAPI* PFN_wglGetExtensionsStringARB)(HDC);
typedef BOOL (WINAPI* PFN_wglChoosePixelFormatARB)(HDC, const int*, const float*, UINT, int*, UINT*);

static PFN_wglCreateContextAttribsARB  wglCreateContextAttribsARB  = nullptr;
static PFN_wglSwapIntervalEXT          wglSwapIntervalEXT          = nullptr;

// ---- Dynamic GL function loader ----
static HMODULE s_openglDll = nullptr;

PROC Win32GL_GetProcAddress(const char* name) {
    if (!s_openglDll) s_openglDll = GetModuleHandleA("opengl32");
    
    // For OpenGL 1.1 core functions, try opengl32.dll first
    // wglGetProcAddress returns NULL for core functions on some implementations
    PROC p = (PROC)GetProcAddress(s_openglDll, name);
    if (!p) {
        p = wglGetProcAddress(name);
    }
    
    if (!p) {
        fprintf(stderr, "[Win32GL] Failed to load GL function: %s\n", name);
    }
    return p;
}

// ---- Forward declarations ----
static LRESULT CALLBACK WGLWndProc(HWND, UINT, WPARAM, LPARAM);
static bool CreateTemporaryContext(HWND* outHwnd, HDC* outHdc, HGLRC* outHglrc);

// ---- Window class registration ----
static const wchar_t* kClassName = L"BlackHoleWGL";

static void RegisterWindowClass() {
    static bool registered = false;
    if (registered) return;

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WGLWndProc;
    wc.hInstance      = GetModuleHandle(nullptr);
    wc.hIcon          = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName  = kClassName;
    RegisterClassExW(&wc);
    registered = true;
}

// GUI active state — when true, don't forward hits as transparent
extern bool GUI_IsActive();

// Forward declaration for ImGui Win32 backend
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---- Window Procedure ----
static Win32GL* g_wgl = nullptr;

static LRESULT CALLBACK WGLWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Let ImGui process input first
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;

    switch (msg) {
    case WM_NCHITTEST:
        // When GUI is active, allow normal window interaction
        if (GUI_IsActive()) {
            return HTCLIENT;
        }
        return HTTRANSPARENT;
    case WM_NCACTIVATE:
        return FALSE;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATEANDEAT;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE && g_wgl) {
            g_wgl->shouldClose = true;
            return 0;
        }
        break;
    case WM_DESTROY:
        if (g_wgl) g_wgl->shouldClose = true;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---- Temporary context for extension loading ----
static bool CreateTemporaryContext(HWND* outHwnd, HDC* outHdc, HGLRC* outHglrc) {
    RegisterWindowClass();

    HWND hwnd = CreateWindowExW(
        0, kClassName, L"BlackHoleTemp",
        WS_POPUP,
        0, 0, 1, 1,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hwnd) return false;

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);

    HGLRC hglrc = wglCreateContext(hdc);
    if (!hglrc) { ReleaseDC(hwnd, hdc); DestroyWindow(hwnd); return false; }

    wglMakeCurrent(hdc, hglrc);

    // Load WGL extensions
    wglCreateContextAttribsARB = (PFN_wglCreateContextAttribsARB)wglGetProcAddress("wglCreateContextAttribsARB");
    wglSwapIntervalEXT = (PFN_wglSwapIntervalEXT)wglGetProcAddress("wglSwapIntervalEXT");

    *outHwnd = hwnd;
    *outHdc = hdc;
    *outHglrc = hglrc;
    return true;
}

// ---- Main Init ----
bool Win32GL_Init(Win32GL* wgl, HMONITOR hmon, int w, int h) {
    if (!wgl) return false;
    g_wgl = wgl;
    wgl->hmon = hmon;
    wgl->width = w;
    wgl->height = h;

    SetProcessDPIAware();

    // If no monitor specified, use primary
    if (!hmon) {
        hmon = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    }

    // Get monitor info for positioning
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hmon, &mi);
    int monX = mi.rcMonitor.left;
    int monY = mi.rcMonitor.top;

    // 1. Create temporary context to load WGL extensions
    HWND tmpHwnd; HDC tmpHdc; HGLRC tmpHglrc;
    if (!CreateTemporaryContext(&tmpHwnd, &tmpHdc, &tmpHglrc)) {
        fprintf(stderr, "[Win32GL] Failed to create temporary GL context\n");
        return false;
    }

    // 2. Register window class
    RegisterWindowClass();

    // 3. Create the real window (initially off-screen to avoid flash)
    HWND hwnd = CreateWindowExW(
        WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        kClassName, L"BlackHole",
        WS_POPUP | WS_VISIBLE,
        -32000, -32000, w, h,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hwnd) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tmpHglrc);
        ReleaseDC(tmpHwnd, tmpHdc);
        DestroyWindow(tmpHwnd);
        fprintf(stderr, "[Win32GL] CreateWindowExW failed: %lu\n", GetLastError());
        return false;
    }

    HDC hdc = GetDC(hwnd);

    // 4. Choose pixel format
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);

    // 5. Create OpenGL 3.3 context using ARB extension
    HGLRC hglrc = nullptr;
    if (wglCreateContextAttribsARB) {
        const int attribs[] = {
            0x2091, 3, // WGL_CONTEXT_MAJOR_VERSION_ARB
            0x2092, 3, // WGL_CONTEXT_MINOR_VERSION_ARB
            0x9126, 2, // WGL_CONTEXT_PROFILE_MASK_ARB = WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
            0
        };
        hglrc = wglCreateContextAttribsARB(hdc, nullptr, attribs);
    }

    // Fallback to default context
    if (!hglrc) {
        hglrc = wglCreateContext(hdc);
    }

    if (!hglrc) {
        fprintf(stderr, "[Win32GL] Failed to create OpenGL context\n");
        ReleaseDC(hwnd, hdc);
        DestroyWindow(hwnd);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tmpHglrc);
        ReleaseDC(tmpHwnd, tmpHdc);
        DestroyWindow(tmpHwnd);
        return false;
    }

    // 6. Switch to the new context, destroy temporary
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(tmpHglrc);
    ReleaseDC(tmpHwnd, tmpHdc);
    DestroyWindow(tmpHwnd);

    wglMakeCurrent(hdc, hglrc);

    // 7. Enable VSync
    if (wglSwapIntervalEXT) {
        wglSwapIntervalEXT(1);
    }

    // 8. DWM attributes for transparency
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = FALSE;
    DwmEnableBlurBehindWindow(hwnd, &bb);

    // Transparent border color (Win11)
    COLORREF borderColor = 0x00000000;
    DwmSetWindowAttribute(hwnd, DWMWINDOWATTRIBUTE(34), &borderColor, sizeof(borderColor)); // DWMWA_BORDER_COLOR

    // Disable NC rendering
    DWORD ncrPolicy = 2; // DWMNCRP_DISABLED
    DwmSetWindowAttribute(hwnd, DWMWINDOWATTRIBUTE(2), &ncrPolicy, sizeof(ncrPolicy)); // DWMWA_NCRENDERING_POLICY

    // Exclude from capture (so WGC doesn't capture our own window)
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);

    // 9. Keep hidden initially
    SetWindowPos(hwnd, HWND_TOPMOST, monX, monY, w, h, SWP_HIDEWINDOW);

    // 10. High-precision timer
    QueryPerformanceFrequency(&wgl->freq);
    QueryPerformanceCounter(&wgl->start);

    // 11. Listen for session changes (lock screen)
    WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);

    wgl->hwnd = hwnd;
    wgl->hdc = hdc;
    wgl->hglrc = hglrc;
    wgl->active = true;
    wgl->shouldClose = false;

    return true;
}

void Win32GL_Shutdown(Win32GL* wgl) {
    if (!wgl || !wgl->hwnd) return;

    WTSUnRegisterSessionNotification(wgl->hwnd);

    if (wgl->hglrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(wgl->hglrc);
    }
    if (wgl->hdc) ReleaseDC(wgl->hwnd, wgl->hdc);
    if (wgl->hwnd) DestroyWindow(wgl->hwnd);

    wgl->hwnd = nullptr;
    wgl->hdc = nullptr;
    wgl->hglrc = nullptr;
    wgl->active = false;
}

void Win32GL_Show(Win32GL* wgl) {
    if (!wgl || !wgl->hwnd) return;

    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(wgl->hmon, &mi);

    SetWindowPos(wgl->hwnd, HWND_TOPMOST,
        mi.rcMonitor.left, mi.rcMonitor.top,
        wgl->width, wgl->height,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);

    // Bring to top after showing
    SetWindowPos(wgl->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void Win32GL_Hide(Win32GL* wgl) {
    if (!wgl || !wgl->hwnd) return;
    SetWindowPos(wgl->hwnd, nullptr, -32000, -32000, 0, 0,
        SWP_NOSIZE | SWP_HIDEWINDOW | SWP_NOACTIVATE);
}

void Win32GL_SwapBuffers(Win32GL* wgl) {
    if (wgl && wgl->hdc) SwapBuffers(wgl->hdc);
}

bool Win32GL_PollEvents(Win32GL* wgl) {
    if (!wgl || wgl->shouldClose) return false;

    MSG msg;
    // Only process messages for our window, not all thread messages
    while (PeekMessageW(&msg, wgl->hwnd, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            wgl->shouldClose = true;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return !wgl->shouldClose;
}

double Win32GL_GetTime(Win32GL* wgl) {
    if (!wgl) return 0.0;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - wgl->start.QuadPart) / (double)wgl->freq.QuadPart;
}

void Win32GL_EnableLayered(Win32GL* wgl) {
    if (!wgl || !wgl->hwnd) return;
    LONG exStyle = GetWindowLongW(wgl->hwnd, GWL_EXSTYLE);
    SetWindowLongW(wgl->hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    SetLayeredWindowAttributes(wgl->hwnd, 0, 255, LWA_ALPHA);
}

void Win32GL_HideSystemCursor() {
    // Replace system cursor with 1x1 transparent cursor
    HCURSOR hCursor = nullptr;
    CURSORINFO ci = {};
    ci.cbSize = sizeof(ci);
    GetCursorInfo(&ci);

    // Create invisible cursor
    int size = GetSystemMetrics(SM_CXCURSOR);
    HDC hdc = GetDC(nullptr);
    HBITMAP hBmpColor = CreateCompatibleBitmap(hdc, size, size);
    HBITMAP hBmpMask = CreateCompatibleBitmap(hdc, size, size);
    ReleaseDC(nullptr, hdc);

    HDC hdcMem = CreateCompatibleDC(nullptr);
    HBITMAP oldBmp = (HBITMAP)SelectObject(hdcMem, hBmpMask);
    PatBlt(hdcMem, 0, 0, size, size, WHITENESS);
    SelectObject(hdcMem, hBmpColor);
    PatBlt(hdcMem, 0, 0, size, size, BLACKNESS);
    SelectObject(hdcMem, oldBmp);
    DeleteDC(hdcMem);

    ICONINFO ii = {};
    ii.fIcon = FALSE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask = hBmpMask;
    ii.hbmColor = hBmpColor;
    hCursor = CreateIconIndirect(&ii);

    DeleteObject(hBmpColor);
    DeleteObject(hBmpMask);

    if (hCursor) {
        SetSystemCursor(hCursor, OCR_NORMAL);
    }
}

void Win32GL_RestoreSystemCursor() {
    SystemParametersInfo(SPI_SETCURSORS, 0, nullptr, 0);
}

void Win32GL_SetClickThrough(Win32GL* wgl, bool enabled) {
    if (!wgl || !wgl->hwnd) return;
    LONG exStyle = GetWindowLongW(wgl->hwnd, GWL_EXSTYLE);
    if (enabled) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongW(wgl->hwnd, GWL_EXSTYLE, exStyle);
}
