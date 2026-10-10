#include "window_devour.h"
#include "win32_gl.h"   // Win32GL_GetProcAddress

#include <dwmapi.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

// ================================================================
// 窗口吞噬实现（计划书 §3）
//
// CPU/GPU 分工：
//   CPU（Update）：螺旋方程（与 desktop_icons.cpp 同源）
//     radius = r0·exp(-rate·p)；th = th0 + spin·p^1.5；scale = radius/r0
//   GPU：顶点级 quad 旋转收缩（公转轨道位置 + 自转），片元级以黑洞为
//     极点的螺旋相位扭曲（"无限扭曲"的观感来源）。
//
// 抓帧时序（计划书 §3.4，无闪烁关键）：
//   枚举 → 全部 PrintWindow → 统一 SW_HIDE → 下一次 Update 绘制碎片。
// ================================================================

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif
#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif

// ---- 参数（main 每帧广播） ----
static float s_rate     = 0.35f;
static float s_spin     = 1.2f;
static float s_warp     = 1.0f;
static float s_stagger  = 0.15f;
static float s_rHide    = 40.0f;
static HWND  s_selfHwnd = nullptr;

// ---- 上限（计划书 R4：显存保护） ----
static const int   MAX_WINDOWS    = 12;    // 单批最多动画窗口数
static const int   MAX_TEX_DIM    = 2048;  // 单窗纹理最大边（超出 2:1 下采样）
static const float MIN_WIN_SIZE   = 80.0f; // 过滤杂项微窗

// ---- 动画记录 ----
struct WinRec {
    HWND          hwnd = nullptr;
    unsigned int  tex  = 0;      // GL 纹理（0 = 未上传/已删除）
    float         cx = 0, cy = 0;   // 窗口中心（屏幕px，抓帧瞬间）
    float         w = 0, h = 0;     // 纹理尺寸（px，可能下采样过）
    float         r0 = 1, th0 = 0;  // 相对黑洞中心的初始极坐标
    float         t0 = 0;           // 错峰起始时刻（模块时间轴）
    bool          active = true;
};
static std::vector<WinRec> s_records;
static float  s_elapsed = 0.0f;   // 模块单调时间（不清零，支持追加批次）

// ================================================================
// 动态 GL 加载（仿 gl_texture.cpp；不依赖 main.cpp 的 static 指针）
// ================================================================
static void (*s_glGenTextures)(int, unsigned int*) = nullptr;
static void (*s_glDeleteTextures)(int, const unsigned int*) = nullptr;
static void (*s_glBindTexture)(unsigned int, unsigned int) = nullptr;
static void (*s_glTexParameteri)(unsigned int, unsigned int, int) = nullptr;
static void (*s_glTexImage2D)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*) = nullptr;
static unsigned int (*s_glCreateShader)(unsigned int) = nullptr;
static void (*s_glShaderSource)(unsigned int, int, const char**, const int*) = nullptr;
static void (*s_glCompileShader)(unsigned int) = nullptr;
static void (*s_glGetShaderiv)(unsigned int, unsigned int, int*) = nullptr;
static void (*s_glGetShaderInfoLog)(unsigned int, int, int*, char*) = nullptr;
static unsigned int (*s_glCreateProgram)(void) = nullptr;
static void (*s_glAttachShader)(unsigned int, unsigned int) = nullptr;
static void (*s_glLinkProgram)(unsigned int) = nullptr;
static void (*s_glGetProgramiv)(unsigned int, unsigned int, int*) = nullptr;
static void (*s_glGetProgramInfoLog)(unsigned int, int, int*, char*) = nullptr;
static unsigned int (*s_glDeleteShader)(unsigned int) = nullptr;
static unsigned int (*s_glDeleteProgram)(unsigned int) = nullptr;
static int  (*s_glGetUniformLocation)(unsigned int, const char*) = nullptr;
static void (*s_glUseProgram)(unsigned int) = nullptr;
static void (*s_glUniform1f)(int, float) = nullptr;
static void (*s_glUniform2f)(int, float, float) = nullptr;
static void (*s_glUniform1i)(int, int) = nullptr;
static void (*s_glGenVertexArrays)(int, unsigned int*) = nullptr;
static void (*s_glDeleteVertexArrays)(int, const unsigned int*) = nullptr;
static void (*s_glBindVertexArray)(unsigned int) = nullptr;
static void (*s_glGenBuffers)(int, unsigned int*) = nullptr;
static void (*s_glDeleteBuffers)(int, const unsigned int*) = nullptr;
static void (*s_glBindBuffer)(unsigned int, unsigned int) = nullptr;
static void (*s_glBufferData)(unsigned int, int, const void*, unsigned int) = nullptr;
static void (*s_glVertexAttribPointer)(unsigned int, int, unsigned int, unsigned char, int, const void*) = nullptr;
static void (*s_glEnableVertexAttribArray)(unsigned int) = nullptr;
static void (*s_glDrawArrays)(unsigned int, int, int) = nullptr;
static bool s_glLoaded = false;

// GL 1.1 固定函数（opengl32.lib 直接链接）
static void (*s_glEnable)(unsigned int) = nullptr;
static void (*s_glDisable)(unsigned int) = nullptr;
static void (*s_glBlendFunc)(unsigned int, unsigned int) = nullptr;
static void (*s_glGetIntegerv)(unsigned int, int*) = nullptr;

static bool LoadGLFunctions() {
    if (s_glLoaded) return true;
    s_glLoaded = true;
    #define LOAD_PTR(dst, name) \
        dst = (decltype(dst))Win32GL_GetProcAddress(name); \
        if (!dst) { fprintf(stderr, "[WinDevour] Missing GL func: %s\n", name); }
    LOAD_PTR(s_glGenTextures,     "glGenTextures");
    LOAD_PTR(s_glDeleteTextures,  "glDeleteTextures");
    LOAD_PTR(s_glBindTexture,     "glBindTexture");
    LOAD_PTR(s_glTexParameteri,   "glTexParameteri");
    LOAD_PTR(s_glTexImage2D,      "glTexImage2D");
    LOAD_PTR(s_glCreateShader,    "glCreateShader");
    LOAD_PTR(s_glShaderSource,    "glShaderSource");
    LOAD_PTR(s_glCompileShader,  "glCompileShader");
    LOAD_PTR(s_glGetShaderiv,     "glGetShaderiv");
    LOAD_PTR(s_glGetShaderInfoLog,"glGetShaderInfoLog");
    LOAD_PTR(s_glCreateProgram,   "glCreateProgram");
    LOAD_PTR(s_glAttachShader,    "glAttachShader");
    LOAD_PTR(s_glLinkProgram,     "glLinkProgram");
    LOAD_PTR(s_glGetProgramiv,    "glGetProgramiv");
    LOAD_PTR(s_glGetProgramInfoLog,"glGetProgramInfoLog");
    LOAD_PTR(s_glDeleteShader,    "glDeleteShader");
    LOAD_PTR(s_glDeleteProgram,   "glDeleteProgram");
    LOAD_PTR(s_glGetUniformLocation, "glGetUniformLocation");
    LOAD_PTR(s_glUseProgram,      "glUseProgram");
    LOAD_PTR(s_glUniform1f,       "glUniform1f");
    LOAD_PTR(s_glUniform2f,       "glUniform2f");
    LOAD_PTR(s_glUniform1i,       "glUniform1i");
    LOAD_PTR(s_glGenVertexArrays, "glGenVertexArrays");
    LOAD_PTR(s_glDeleteVertexArrays, "glDeleteVertexArrays");
    LOAD_PTR(s_glBindVertexArray, "glBindVertexArray");
    LOAD_PTR(s_glGenBuffers,      "glGenBuffers");
    LOAD_PTR(s_glDeleteBuffers,   "glDeleteBuffers");
    LOAD_PTR(s_glBindBuffer,      "glBindBuffer");
    LOAD_PTR(s_glBufferData,      "glBufferData");
    LOAD_PTR(s_glVertexAttribPointer, "glVertexAttribPointer");
    LOAD_PTR(s_glEnableVertexAttribArray, "glEnableVertexAttribArray");
    LOAD_PTR(s_glDrawArrays,      "glDrawArrays");
    LOAD_PTR(s_glEnable,          "glEnable");
    LOAD_PTR(s_glDisable,         "glDisable");
    LOAD_PTR(s_glBlendFunc,       "glBlendFunc");
    LOAD_PTR(s_glGetIntegerv,     "glGetIntegerv");
    #undef LOAD_PTR
    // 关键函数缺失检查（纹理/绘制/着色器三组任缺即禁用）
    bool ok = s_glGenTextures && s_glTexImage2D && s_glBindTexture && s_glDeleteTextures
           && s_glCreateShader && s_glShaderSource && s_glCompileShader && s_glCreateProgram
           && s_glLinkProgram && s_glGetUniformLocation && s_glUseProgram
           && s_glGenVertexArrays && s_glBindVertexArray && s_glGenBuffers && s_glBindBuffer
           && s_glBufferData && s_glVertexAttribPointer && s_glEnableVertexAttribArray
           && s_glDrawArrays && s_glEnable && s_glDisable && s_glBlendFunc;
    return ok;
}

// ================================================================
// shader program + quad
// ================================================================
static unsigned int s_program = 0;
static unsigned int s_vao = 0, s_vbo = 0;
static bool s_shaderOk = false;

// uniform locations
static int uCenter = -1, uHalfSize = -1, uRot = -1, uScreen = -1,
           uHole = -1, uWarp = -1, uTex = -1;

static std::string ReadFileStr(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return "";
    std::string out;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size > 0) {
        out.resize((size_t)size);
        size_t rd = fread(&out[0], 1, (size_t)size, f);
        out.resize(rd);
    }
    fclose(f);
    return out;
}

static bool CompileShaderObj(unsigned int shader, const char* label) {
    s_glCompileShader(shader);
    int status = 0;
    s_glGetShaderiv(shader, 0x8B81/*GL_COMPILE_STATUS*/, &status);
    if (!status) {
        char log[1024];
        s_glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        fprintf(stderr, "[WinDevour] %s compile failed: %s\n", label, log);
        return false;
    }
    return true;
}

static bool InitShaderAndQuad() {
    std::string vs = ReadFileStr("shaders/window_vortex_vert.glsl");
    std::string fs = ReadFileStr("shaders/window_vortex_frag.glsl");
    if (vs.empty() || fs.empty()) {
        fprintf(stderr, "[WinDevour] shader files missing (vert=%zu frag=%zu bytes)\n",
                vs.size(), fs.size());
        return false;
    }

    const char* vsSrc = vs.c_str();
    unsigned int vsh = s_glCreateShader(0x8B31/*GL_VERTEX_SHADER*/);
    s_glShaderSource(vsh, 1, &vsSrc, nullptr);
    bool vOk = CompileShaderObj(vsh, "window_vortex_vert");

    const char* fsSrc = fs.c_str();
    unsigned int fsh = s_glCreateShader(0x8B30/*GL_FRAGMENT_SHADER*/);
    s_glShaderSource(fsh, 1, &fsSrc, nullptr);
    bool fOk = CompileShaderObj(fsh, "window_vortex_frag");

    if (!vOk || !fOk) {
        s_glDeleteShader(vsh);
        s_glDeleteShader(fsh);
        return false;
    }

    s_program = s_glCreateProgram();
    s_glAttachShader(s_program, vsh);
    s_glAttachShader(s_program, fsh);
    s_glLinkProgram(s_program);
    int linked = 0;
    s_glGetProgramiv(s_program, 0x8B82/*GL_LINK_STATUS*/, &linked);
    s_glDeleteShader(vsh);
    s_glDeleteShader(fsh);
    if (!linked) {
        char log[1024];
        s_glGetProgramInfoLog(s_program, sizeof(log), nullptr, log);
        fprintf(stderr, "[WinDevour] program link failed: %s\n", log);
        s_glDeleteProgram(s_program);
        s_program = 0;
        return false;
    }

    uCenter   = s_glGetUniformLocation(s_program, "uCenter");
    uHalfSize = s_glGetUniformLocation(s_program, "uHalfSize");
    uRot      = s_glGetUniformLocation(s_program, "uRot");
    uScreen   = s_glGetUniformLocation(s_program, "uScreen");
    uHole     = s_glGetUniformLocation(s_program, "uHole");
    uWarp     = s_glGetUniformLocation(s_program, "uWarp");
    uTex      = s_glGetUniformLocation(s_program, "uTex");

    // 全屏 quad（TRIANGLE_STRIP 4 顶点，-1..1），供全部窗口 draw 复用
    const float quad[] = { -1.0f, -1.0f,   1.0f, -1.0f,   -1.0f, 1.0f,   1.0f, 1.0f };
    s_glGenVertexArrays(1, &s_vao);
    s_glBindVertexArray(s_vao);
    s_glGenBuffers(1, &s_vbo);
    s_glBindBuffer(0x8892/*GL_ARRAY_BUFFER*/, s_vbo);
    s_glBufferData(0x8892/*GL_ARRAY_BUFFER*/, sizeof(quad), quad, 0x88E4/*GL_STATIC_DRAW*/);
    s_glVertexAttribPointer(0, 2, 0x1406/*GL_FLOAT*/, 0, sizeof(float) * 2, (const void*)0);
    s_glEnableVertexAttribArray(0);
    s_glBindVertexArray(0);

    printf("[WinDevour] shader program linked OK\n");
    return true;
}

// ================================================================
// 枚举与过滤（计划书 §3.5）
// ================================================================
static const wchar_t* kExcludedClasses[] = {
    L"Progman", L"WorkerW", L"SHELLDLL_DefView",
    L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
    L"tooltips_class32", L"Windows.UI.Core.CoreWindow",
    L"Xaml_WindowedImmutableClass", L"MSCTFIME UI", L"Default IME",
};

struct EnumCtx {
    float holeX = 0, holeY = 0;
    RECT  monitor{};                 // 黑洞所在显示器（窗口须与之相交）
    std::vector<HWND> found;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lp) {
    EnumCtx* ctx = reinterpret_cast<EnumCtx*>(lp);

    if (hwnd == s_selfHwnd) return TRUE;                    // 不吞自己
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

    wchar_t cls[64] = {};
    if (GetClassNameW(hwnd, cls, 64) > 0) {
        for (const wchar_t* ex : kExcludedClasses) {
            if (wcscmp(cls, ex) == 0) return TRUE;
        }
    }

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;            // 工具悬浮窗

    // UWP 挂起（cloaked）窗口抓帧是黑的
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
        && cloaked != 0) {
        return TRUE;
    }

    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) return TRUE;
    float w = (float)(rc.right - rc.left);
    float h = (float)(rc.bottom - rc.top);
    if (w < MIN_WIN_SIZE || h < MIN_WIN_SIZE) return TRUE;  // 杂项微窗

    // 与黑洞所在显示器相交（多显示器：不吞别的屏的窗口）
    RECT inter{};
    if (!IntersectRect(&inter, &rc, &ctx->monitor)) return TRUE;

    ctx->found.push_back(hwnd);
    return TRUE;
}

// ================================================================
// 抓帧（计划书 §3.4 + T2）
// ================================================================

// 黑帧检测：均匀采样 256 点，RGB 全 0 → 判黑帧（DRM/独占全屏，计划书 R1）
static bool IsBlankFrame(const unsigned char* bgra, int w, int h, int stride) {
    int samples = 0, black = 0;
    int stepX = w / 16, stepY = h / 16;
    if (stepX < 1) stepX = 1;
    if (stepY < 1) stepY = 1;
    for (int y = stepY / 2; y < h; y += stepY) {
        for (int x = stepX / 2; x < w; x += stepX) {
            const unsigned char* px = bgra + (size_t)y * stride + (size_t)x * 4;
            if (px[0] == 0 && px[1] == 0 && px[2] == 0) black++;
            samples++;
        }
    }
    return samples > 0 && black == samples;
}

// 超过 MAX_TEX_DIM 时 2:1 步长下采样（nearest；扭曲动画中无可见质量损失）
static void Downsample2x(unsigned char* dst, int& w, int& h,
                         const unsigned char* src, int srcStride) {
    int nw = w / 2, nh = h / 2;
    for (int y = 0; y < nh; y++) {
        const unsigned char* srow0 = src + (size_t)(y * 2) * srcStride;
        const unsigned char* srow1 = src + (size_t)(y * 2 + 1) * srcStride;
        unsigned char* drow = dst + (size_t)y * (nw * 4);
        for (int x = 0; x < nw; x++) {
            const unsigned char* p0 = srow0 + (size_t)(x * 2) * 4;
            const unsigned char* p1 = srow1 + (size_t)(x * 2) * 4;
            for (int c = 0; c < 4; c++) drow[x * 4 + c] = (unsigned char)(
                ((int)p0[c] + (int)p0[4 + c] + (int)p1[c] + (int)p1[4 + c]) / 4);
        }
    }
    w = nw;
    h = nh;
}

// 抓单窗到 BGRA 缓冲（bottom-up DIB：行序与 GL v 轴同向，见计划书 T2）。
// 成功时 *outW/*outH 为（可能下采样后的）尺寸，buffer 返回新分配内存。
static unsigned char* CaptureWindow(HWND hwnd, int* outW, int* outH) {
    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) return nullptr;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return nullptr;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = h;      // 正高度 = bottom-up（GL v=0 对应窗口底部）
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) { if (dib) DeleteObject(dib); return nullptr; }

    HDC hdcMem = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(hdcMem, dib);
    BOOL ok = PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT);
    SelectObject(hdcMem, old);
    DeleteDC(hdcMem);
    if (!ok) { DeleteObject(dib); return nullptr; }

    int stride = w * 4;
    const unsigned char* src = (const unsigned char*)bits;

    // 黑帧（DRM/独占全屏）：调用方据此跳过动画
    if (IsBlankFrame(src, w, h, stride)) {
        DeleteObject(dib);
        *outW = *outH = 0;
        return nullptr;
    }

    unsigned char* result = nullptr;
    if (w > MAX_TEX_DIM || h > MAX_TEX_DIM) {
        // 反复 2:1 直到进上限
        int cw = w, ch = h;
        unsigned char* cur = new unsigned char[(size_t)cw * ch * 4];
        memcpy(cur, src, (size_t)cw * ch * 4);
        while (cw > MAX_TEX_DIM || ch > MAX_TEX_DIM) {
            int nw = cw / 2, nh = ch / 2;
            unsigned char* next = new unsigned char[(size_t)nw * nh * 4];
            Downsample2x(next, cw, ch, cur, cw * 4);   // 注意：Downsample2x 内部更新 cw/ch
            delete[] cur;
            cur = next;
        }
        result = cur;
        w = cw; h = ch;
    } else {
        result = new unsigned char[(size_t)w * h * 4];
        memcpy(result, src, (size_t)w * h * 4);
    }

    DeleteObject(dib);
    *outW = w;
    *outH = h;
    return result;
}

// ================================================================
// 对外接口
// ================================================================
bool WindowDevour_Init(HWND selfHwnd) {
    s_selfHwnd = selfHwnd;
    if (!LoadGLFunctions()) {
        fprintf(stderr, "[WinDevour] GL functions unavailable, module disabled\n");
        return false;
    }
    s_shaderOk = InitShaderAndQuad();
    return true;   // shader 失败仍返回 true：Trigger 走纯 SW_HIDE 降级路径
}

void WindowDevour_SetParams(float rate, float spin, float warp, float stagger) {
    s_rate    = rate    > 0.0f ? rate    : 0.05f;
    s_spin    = spin;
    s_warp    = warp    > 0.0f ? warp    : 0.0f;
    s_stagger = stagger > 0.0f ? stagger : 0.0f;
}

void WindowDevour_SetRHide(float radiusPx) {
    s_rHide = radiusPx > 10.0f ? radiusPx : 10.0f;
}

int WindowDevour_Trigger(float holeScreenX, float holeScreenY) {
    // 清理已完成记录（纹理已删，仅剩尸体）
    s_records.erase(
        std::remove_if(s_records.begin(), s_records.end(),
                       [](const WinRec& r) { return !r.active; }),
        s_records.end());

    // 枚举
    EnumCtx ctx;
    ctx.holeX = holeScreenX;
    ctx.holeY = holeScreenY;
    HMONITOR hmon = MonitorFromPoint(POINT{ (LONG)holeScreenX, (LONG)holeScreenY },
                                     MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hmon, &mi)) {
        ctx.monitor = mi.rcMonitor;
    } else {
        ctx.monitor = RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    }
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    if (ctx.found.empty()) return 0;

    // 面积降序 + 截断（R4：最多 MAX_WINDOWS 个动画窗）
    std::sort(ctx.found.begin(), ctx.found.end(), [](HWND a, HWND b) {
        RECT ra{}, rb{};
        GetWindowRect(a, &ra);
        GetWindowRect(b, &rb);
        LONG areaA = (ra.right - ra.left) * (ra.bottom - ra.top);
        LONG areaB = (rb.right - rb.left) * (rb.bottom - rb.top);
        return areaA > areaB;
    });
    if ((int)ctx.found.size() > MAX_WINDOWS) ctx.found.resize(MAX_WINDOWS);

    // 逐窗抓帧（此时全部真窗口仍可见，计划书 §3.4 时序）
    struct Captured {
        HWND hwnd;
        unsigned char* bgra;
        int w, h;
        float cx, cy;
        bool anim;       // false = 黑帧，跳过动画但仍要隐藏
    };
    std::vector<Captured> caps;
    for (HWND hwnd : ctx.found) {
        RECT rc{};
        if (!GetWindowRect(hwnd, &rc)) continue;
        int w = 0, h = 0;
        unsigned char* bgra = s_shaderOk ? CaptureWindow(hwnd, &w, &h) : nullptr;
        Captured c = {};
        c.hwnd = hwnd;
        c.bgra = bgra;
        c.w = w;
        c.h = h;
        c.cx = ((float)rc.left + (float)rc.right) * 0.5f;
        c.cy = ((float)rc.top + (float)rc.bottom) * 0.5f;
        c.anim = (bgra != nullptr);
        caps.push_back(c);
    }

    // 统一隐藏（含黑帧窗口：吞掉语义仍成立，只是无动画）
    int eaten = 0;
    for (const Captured& c : caps) {
        if (ShowWindow(c.hwnd, SW_HIDE)) eaten++;
    }
    if (eaten == 0) {
        for (Captured& c : caps) delete[] c.bgra;
        return 0;
    }

    // 建动画记录（追加批次：t0 基于当前 s_elapsed 错峰）
    float tBase = s_elapsed;
    int idx = 0;
    int added = 0;
    for (const Captured& c : caps) {
        if (!c.anim || !c.bgra) continue;
        WinRec rec = {};
        rec.hwnd = c.hwnd;
        rec.cx = c.cx;
        rec.cy = c.cy;
        rec.w = (float)c.w;
        rec.h = (float)c.h;
        float dx = c.cx - holeScreenX;
        float dy = c.cy - holeScreenY;
        rec.r0 = sqrtf(dx * dx + dy * dy);
        if (rec.r0 < 1.0f) rec.r0 = 1.0f;   // 窗口恰在洞心：给个最小半径防退化
        rec.th0 = atan2f(dy, dx);
        rec.t0 = tBase + s_stagger * (float)idx;
        rec.active = true;

        // 上传纹理（GL_RGBA8 + BGRA 无符号字节）
        s_glGenTextures(1, &rec.tex);
        s_glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, rec.tex);
        s_glTexParameteri(0x0DE1, 0x2801/*GL_TEXTURE_MIN_FILTER*/, 0x2601/*GL_LINEAR*/);
        s_glTexParameteri(0x0DE1, 0x2800/*GL_TEXTURE_MAG_FILTER*/, 0x2601/*GL_LINEAR*/);
        s_glTexParameteri(0x0DE1, 0x2802/*GL_TEXTURE_WRAP_S*/, 0x812F/*GL_CLAMP_TO_EDGE*/);
        s_glTexParameteri(0x0DE1, 0x2803/*GL_TEXTURE_WRAP_T*/, 0x812F/*GL_CLAMP_TO_EDGE*/);
        s_glTexImage2D(0x0DE1, 0, 0x8058/*GL_RGBA8*/, c.w, c.h, 0,
                       0x80E1/*GL_BGRA*/, 0x1401/*GL_UNSIGNED_BYTE*/, c.bgra);
        s_glBindTexture(0x0DE1, 0);

        delete[] c.bgra;
        s_records.push_back(rec);
        idx++;
        added++;
    }

    printf("[WinDevour] Trigger: %zu windows eaten (%d animated)\n", caps.size(), added);
    return (int)caps.size();
}

void WindowDevour_Update(float holeScreenX, float holeScreenY, float dt) {
    if (s_records.empty()) return;
    s_elapsed += dt;

    // ---- CPU 螺旋状态机（与 desktop_icons.cpp 同方程） ----
    bool anyActive = false;
    for (WinRec& r : s_records) {
        if (!r.active) continue;
        float p = s_elapsed - r.t0;
        if (p < 0.0f) p = 0.0f;   // 错峰等待期：原位静止
        float radius = r.r0 * expf(-s_rate * p);
        if (radius < s_rHide) {
            // 进入视界：删纹理 + 停画（黑洞阴影遮挡消失点，计划书 §3.2）
            if (r.tex) { s_glDeleteTextures(1, &r.tex); r.tex = 0; }
            r.active = false;
            continue;
        }
        anyActive = true;
    }
    if (!anyActive) {
        s_records.clear();
        return;
    }

    // ---- 渲染 ----
    if (!s_shaderOk || !s_program) return;

    int viewport[4] = {};
    s_glGetIntegerv(0x0BA2/*GL_VIEWPORT*/, viewport);
    float screenW = (float)viewport[2];
    float screenH = (float)viewport[3];
    if (screenW < 1.0f || screenH < 1.0f) return;

    // 屏幕坐标 → GL 窗口局部坐标（多显示器：holeScreen 含显示器偏移，
    // NDC 是窗口局部的，不减原点则副屏上碎片整体错位）。
    // r0/th0 是相对量（窗口-黑洞），不依赖原点，无需换算。
    RECT glRc{};
    GetWindowRect(s_selfHwnd, &glRc);
    float localHoleX = holeScreenX - (float)glRc.left;
    float localHoleY = holeScreenY - (float)glRc.top;

    // 保存外部混合/深度状态，渲染完还原（黑洞场景可能依赖加色混合与深度测试）
    int oldBlend = 0;
    s_glGetIntegerv(0x0BE2/*GL_BLEND*/, &oldBlend);
    int oldSrc = 0, oldDst = 0;
    s_glGetIntegerv(0x0BE7/*GL_BLEND_SRC_RGB*/, &oldSrc);
    s_glGetIntegerv(0x0BE8/*GL_BLEND_DST_RGB*/, &oldDst);
    int oldDepth = 0;
    s_glGetIntegerv(0x0B71/*GL_DEPTH_TEST*/, &oldDepth);

    s_glEnable(0x0BE2/*GL_BLEND*/);
    s_glBlendFunc(0x0302/*GL_SRC_ALPHA*/, 0x0303/*GL_ONE_MINUS_SRC_ALPHA*/);
    s_glDisable(0x0B71/*GL_DEPTH_TEST*/);

    s_glUseProgram(s_program);
    s_glBindVertexArray(s_vao);

    if (uScreen >= 0) s_glUniform2f(uScreen, screenW, screenH);
    // uHole/uCenter 均为 GL 窗口局部像素坐标（y 向下），与 shader 约定一致
    if (uHole >= 0)   s_glUniform2f(uHole, localHoleX, localHoleY);

    for (const WinRec& r : s_records) {
        if (!r.active || !r.tex) continue;
        float p = s_elapsed - r.t0;
        if (p < 0.0f) p = 0.0f;

        float radius = r.r0 * expf(-s_rate * p);
        float th     = r.th0 + s_spin * powf(p, 1.5f);
        float scale  = radius / r.r0;
        if (scale < 0.05f) scale = 0.05f;

        // 局部像素坐标系下的碎片中心：p=0 时恰好回到抓帧时的原位
        // （localHole + r0·cos(th0) = 窗口中心 - GL 窗口原点）
        float cx = localHoleX + radius * cosf(th);
        float cy = localHoleY + radius * sinf(th);
        float halfW = r.w * 0.5f * scale;
        float halfH = r.h * 0.5f * scale;
        float rot   = s_spin * powf(p, 1.5f) * 0.7f;   // 自转略缓于公转
        float warp  = powf(p, 1.5f) * s_warp;

        if (uCenter >= 0)   s_glUniform2f(uCenter, cx, cy);
        if (uHalfSize >= 0) s_glUniform2f(uHalfSize, halfW, halfH);
        if (uRot >= 0)      s_glUniform1f(uRot, rot);
        if (uWarp >= 0)     s_glUniform1f(uWarp, warp);
        if (uTex >= 0)      s_glUniform1i(uTex, 0);

        s_glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, r.tex);
        s_glDrawArrays(0x0005/*GL_TRIANGLE_STRIP*/, 0, 4);
    }

    s_glBindVertexArray(0);
    s_glBindTexture(0x0DE1, 0);
    s_glUseProgram(0);

    // 还原混合/深度状态
    if (oldBlend) s_glEnable(0x0BE2/*GL_BLEND*/); else s_glDisable(0x0BE2/*GL_BLEND*/);
    s_glBlendFunc((unsigned int)oldSrc, (unsigned int)oldDst);
    if (oldDepth) s_glEnable(0x0B71/*GL_DEPTH_TEST*/); else s_glDisable(0x0B71/*GL_DEPTH_TEST*/);
}

bool WindowDevour_HasActive() {
    for (const WinRec& r : s_records) {
        if (r.active) return true;
    }
    return false;
}

void WindowDevour_Shutdown() {
    for (WinRec& r : s_records) {
        if (r.tex) s_glDeleteTextures(1, &r.tex);
    }
    s_records.clear();
    if (s_vbo)     { s_glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
    if (s_vao)     { s_glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
    if (s_program) { s_glDeleteProgram(s_program); s_program = 0; }
    s_shaderOk = false;
}
