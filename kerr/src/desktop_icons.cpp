#include "desktop_icons.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

// ---- ListView messages / styles (winuser 常量手写，避免额外 include) ----
// 下列取值已对照 MSYS2 UCRT64 的 commctrl.h 实测校验，勿凭记忆改动。
static const UINT LVM_FIRST_                    = 0x1000;
static const UINT LVM_GETITEMCOUNT_             = LVM_FIRST_ + 4;    // 0x1004
static const UINT LVM_SETITEMPOSITION_          = LVM_FIRST_ + 15;   // 0x100F
static const UINT LVM_GETITEMPOSITION_          = LVM_FIRST_ + 16;   // 0x1010
static const UINT LVM_SETITEMPOSITION32_        = LVM_FIRST_ + 49;   // 0x1031
static const UINT LVM_SETEXTENDEDLISTVIEWSTYLE_ = LVM_FIRST_ + 54;   // 0x1036
static const UINT LVM_GETEXTENDEDLISTVIEWSTYLE_ = LVM_FIRST_ + 55;   // 0x1037

static const LONG_PTR LVS_AUTOARRANGE_   = 0x0100;      // LVS_AUTOARRANGE
static const DWORD    LVS_EX_SNAPTOGRID_ = 0x00080000;  // LVS_EX_SNAPTOGRID

// 每帧最多下发多少个图标（其余下帧继续），防止 explorer 消息洪泛。
static const int MAX_ICONS_PER_TICK = 64;
// 动态发现间隔（秒）。
static const float DISCOVER_INTERVAL = 1.0f;
// 图标上限，防止极端情况下的内存/消息失控。
static const int MAX_ICONS = 2000;

struct IconRec {
    int   index = 0;      // ListView 索引
    int   sx = 0, sy = 0; // 快照位置（ListView 客户区坐标）
    float r0 = 0.0f;      // 相对黑洞中心的初始半径
    float th0 = 0.0f;     // 初始角
    float t0 = 0.0f;      // 错峰启动延迟（秒）
    int   lastX = 0, lastY = 0; // 上次下发的目标位置（用于 <1px 阈值）
    bool  inited = false; // r0/th0/t0 是否已计算
    bool  active = true;  // 是否仍在移动（进入 r_hide 后置 false）
};

// ---- 文件内 static 单例（仅主线程使用） ----
static HWND   s_lv           = nullptr;
static DWORD  s_lvPid        = 0;
static HANDLE s_proc         = nullptr;
static POINT  s_lvOrigin     = { 0, 0 };      // SysListView32 客户区原点（屏幕坐标）
static LPVOID s_remotePoint  = nullptr;       // explorer 进程内的 POINT 缓冲（跨进程编组）
static bool   s_attached     = false;
static bool   s_devouring    = false;

static std::vector<IconRec> s_icons;
static int    s_lastCount    = -1;

static float  s_elapsed      = 0.0f;  // 吞噬开始以来的单调时间
static float  s_sendAccum    = 0.0f;  // 限频累积
static float  s_discoverAccum= 0.0f;
static float  s_lastCx       = 0.0f;  // 最近一次黑洞中心（ListView 客户区坐标）
static float  s_lastCy       = 0.0f;
static bool   s_originInit   = false;

// 被临时修改的样式原值
static LONG_PTR s_savedStyle       = 0;
static DWORD    s_savedExtStyle    = 0;
static bool     s_styleModified    = false;
static bool     s_extStyleModified = false;

// 参数
static float s_rate       = 0.35f;
static float s_spin       = 1.2f;
static float s_stagger    = 0.15f;
static int   s_throttleHz = 30;
static bool  s_restore    = true;
static float s_rHide      = 40.0f;

static char s_lastError[256] = "OK";

static void SetError(const char* msg) {
    if (!msg) msg = "unknown error";
    snprintf(s_lastError, sizeof(s_lastError), "%s", msg);
}

// ------------------------------------------------------------------ 定位链 --
struct EnumLvCtx { HWND lv; };

static BOOL CALLBACK FindWorkerWProc(HWND hwnd, LPARAM lp) {
    wchar_t cls[64] = {};
    if (GetClassNameW(hwnd, cls, 64) == 0) return TRUE;
    if (wcscmp(cls, L"WorkerW") != 0) return TRUE;
    HWND defView = FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
    if (!defView) return TRUE;
    HWND lv = FindWindowExW(defView, nullptr, L"SysListView32", nullptr);
    if (!lv) return TRUE;
    reinterpret_cast<EnumLvCtx*>(lp)->lv = lv;
    return FALSE; // 找到即停止
}

// Why：Progman 下不一定有 SHELLDLL_DefView（Win11 / 壁纸轮播会把它挪到某个 WorkerW 下），
// 因此先试 Progman，再回退枚举 WorkerW。
static HWND FindDesktopListView() {
    HWND progman = FindWindowW(L"Progman", nullptr);
    if (progman) {
        HWND defView = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);
        if (defView) {
            HWND lv = FindWindowExW(defView, nullptr, L"SysListView32", nullptr);
            if (lv) return lv;
        }
    }
    EnumLvCtx ctx = { nullptr };
    EnumWindows(FindWorkerWProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.lv;
}

// ---------------------------------------------------------- 跨进程消息编组 --
static LRESULT SendLvTimeout(UINT msg, WPARAM wp, LPARAM lp, UINT timeoutMs) {
    DWORD_PTR result = 0;
    return SendMessageTimeoutW(s_lv, msg, wp, lp,
                               SMTO_ABORTIFHUNG | SMTO_NORMAL, timeoutMs, &result);
}

// LVM_GETITEMPOSITION 需要 POINT* 指向 explorer 地址空间 → 读远程内存。
static bool ReadItemPosition(int index, POINT* out) {
    if (!s_proc || !s_remotePoint || !out) return false;
    if (SendLvTimeout(LVM_GETITEMPOSITION_, (WPARAM)index, (LPARAM)s_remotePoint, 100) == 0) {
        return false;
    }
    SIZE_T read = 0;
    return ReadProcessMemory(s_proc, s_remotePoint, out, sizeof(POINT), &read) != 0
           && read == sizeof(POINT);
}

// Why：使用 LVM_SETITEMPOSITION32 —— 其 lParam 是 POINT*（MSDN），
// 可表达 32 位含负坐标（多显示器副屏在主屏左侧时坐标为负），
// 而 LVM_SETITEMPOSITION 的 16 位 MAKELPARAM 会被裁剪到 ±32767。
// 该指针仍需指向 explorer 地址空间，故沿用同一个远程 POINT 缓冲。
static bool WriteItemPosition(int index, int x, int y) {
    if (!s_proc || !s_remotePoint) return false;
    POINT pt = { x, y };
    SIZE_T written = 0;
    if (!WriteProcessMemory(s_proc, s_remotePoint, &pt, sizeof(POINT), &written)
        || written != sizeof(POINT)) {
        return false;
    }
    return SendLvTimeout(LVM_SETITEMPOSITION32_, (WPARAM)index, (LPARAM)s_remotePoint, 20) != 0;
}

// ------------------------------------------------------------------ Attach --
bool DesktopIcons_Attach() {
    if (s_attached) return true;

    HWND lv = FindDesktopListView();
    if (!lv) {
        SetError("未找到桌面 SysListView32（explorer 是否在运行？）");
        return false;
    }
    s_lv = lv;
    GetWindowThreadProcessId(lv, &s_lvPid);

    s_proc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
                             PROCESS_QUERY_INFORMATION,
                         FALSE, s_lvPid);
    if (!s_proc) {
        SetError("OpenProcess(explorer) 失败（完整性级别/UIPI 拦截？）");
        s_lv = nullptr;
        return false;
    }

    s_remotePoint = VirtualAllocEx(s_proc, nullptr, sizeof(POINT),
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!s_remotePoint) {
        SetError("VirtualAllocEx 失败（跨进程读位置编组不可用）");
        CloseHandle(s_proc);
        s_proc = nullptr;
        s_lv = nullptr;
        return false;
    }

    s_lvOrigin = POINT{ 0, 0 };
    ClientToScreen(lv, &s_lvOrigin);

    // 记录并尝试移除 LVS_AUTOARRANGE（否则 explorer 会把图标弹回，与动画持续打架）。
    s_savedStyle = GetWindowLongPtrW(lv, GWL_STYLE);
    if (s_savedStyle & LVS_AUTOARRANGE_) {
        SetLastError(0);
        SetWindowLongPtrW(lv, GWL_STYLE, s_savedStyle & ~LVS_AUTOARRANGE_);
        LONG_PTR now = GetWindowLongPtrW(lv, GWL_STYLE);
        if (now & LVS_AUTOARRANGE_) {
            SetError("自动排列(LVS_AUTOARRANGE)移除失败，图标可能被持续弹回");
        } else {
            s_styleModified = true;
        }
    }

    // 扩展样式：清 SNAPTOGRID（wParam=mask, lParam=新值，无需内存编组）。
    DWORD_PTR ext = 0;
    DWORD_PTR r = SendMessageTimeoutW(lv, LVM_GETEXTENDEDLISTVIEWSTYLE_, 0, 0,
                                      SMTO_ABORTIFHUNG | SMTO_NORMAL, 100, &ext);
    if (r != 0) {
        s_savedExtStyle = (DWORD)ext;
        if (s_savedExtStyle & LVS_EX_SNAPTOGRID_) {
            SendMessageTimeoutW(lv, LVM_SETEXTENDEDLISTVIEWSTYLE_,
                                (WPARAM)LVS_EX_SNAPTOGRID_, (LPARAM)0,
                                SMTO_ABORTIFHUNG | SMTO_NORMAL, 100, &r);
            DWORD_PTR after = 0;
            SendMessageTimeoutW(lv, LVM_GETEXTENDEDLISTVIEWSTYLE_, 0, 0,
                                SMTO_ABORTIFHUNG | SMTO_NORMAL, 100, &after);
            if (((DWORD)after) & LVS_EX_SNAPTOGRID_) {
                SetError("对齐到网格(LVS_EX_SNAPTOGRID)移除失败");
            } else {
                s_extStyleModified = true;
            }
        }
    }

    s_attached = true;
    if (s_lastError[0] == '\0' || strcmp(s_lastError, "OK") == 0) SetError("OK");
    return true;
}

void DesktopIcons_Detach() {
    // 恢复被临时改动的样式（即使图标位置要保留，样式也必须还原）。
    if (s_attached && s_lv && IsWindow(s_lv)) {
        if (s_styleModified) {
            SetWindowLongPtrW(s_lv, GWL_STYLE, s_savedStyle);
            s_styleModified = false;
        }
        if (s_extStyleModified) {
            DWORD_PTR r = 0;
            SendMessageTimeoutW(s_lv, LVM_SETEXTENDEDLISTVIEWSTYLE_,
                                (WPARAM)LVS_EX_SNAPTOGRID_,
                                (LPARAM)(s_savedExtStyle & LVS_EX_SNAPTOGRID_),
                                SMTO_ABORTIFHUNG | SMTO_NORMAL, 100, &r);
            s_extStyleModified = false;
        }
    }
    if (s_remotePoint && s_proc) {
        VirtualFreeEx(s_proc, s_remotePoint, 0, MEM_RELEASE);
    }
    s_remotePoint = nullptr;
    if (s_proc) CloseHandle(s_proc);
    s_proc = nullptr;
    s_lv = nullptr;
    s_lvPid = 0;
    s_attached = false;
}

bool DesktopIcons_IsAttached() { return s_attached; }

// --------------------------------------------------------------- Snapshot ---
static int QueryItemCount() {
    if (!s_lv) return -1;
    DWORD_PTR r = 0;
    LRESULT ok = SendMessageTimeoutW(s_lv, LVM_GETITEMCOUNT_, 0, 0,
                                     SMTO_ABORTIFHUNG | SMTO_NORMAL, 100, &r);
    return ok ? (int)r : -1;
}

bool DesktopIcons_Snapshot() {
    if (!s_attached || !s_proc) {
        SetError("Snapshot 前未 Attach");
        return false;
    }
    int count = QueryItemCount();
    if (count < 0) {
        SetError("LVM_GETITEMCOUNT 失败");
        return false;
    }
    if (count > MAX_ICONS) count = MAX_ICONS;

    std::vector<IconRec> recs;
    recs.reserve((size_t)count);
    for (int i = 0; i < count; i++) {
        POINT p = { 0, 0 };
        if (!ReadItemPosition(i, &p)) continue; // 单点失败跳过，不中断整体
        IconRec r;
        r.index = i;
        r.sx = p.x;
        r.sy = p.y;
        r.lastX = p.x;
        r.lastY = p.y;
        recs.push_back(r);
    }
    s_icons.swap(recs);
    s_lastCount = count;
    s_originInit = false;
    SetError("OK");
    return !s_icons.empty();
}

// ---------------------------------------------------------------- Restore ---
static bool RestoreInternal() {
    bool any = false;
    for (size_t i = 0; i < s_icons.size(); i++) {
        if (!s_lv || !IsWindow(s_lv)) break;
        const IconRec& r = s_icons[i];
        WriteItemPosition(r.index, r.sx, r.sy);
        any = true;
    }
    return any;
}

bool DesktopIcons_Restore() {
    if (s_icons.empty()) {
        SetError("没有可还原的快照");
        return false;
    }
    if (s_attached) {
        bool ok = RestoreInternal();
        if (ok) SetError("OK");
        return ok;
    }
    // 已 Detach 时仍允许"立即复位"：临时附加写回后释放。
    if (!DesktopIcons_Attach()) return false;
    bool ok = RestoreInternal();
    DesktopIcons_Detach();
    if (ok) SetError("OK");
    return ok;
}

// -------------------------------------------------------- 螺旋参数初始化 ---
static void InitOriginForAll(float holeScreenX, float holeScreenY) {
    float Cx = holeScreenX - (float)s_lvOrigin.x;
    float Cy = holeScreenY - (float)s_lvOrigin.y;
    s_lastCx = Cx;
    s_lastCy = Cy;

    for (size_t i = 0; i < s_icons.size(); i++) {
        IconRec& r = s_icons[i];
        float dx = (float)r.sx - Cx;
        float dy = (float)r.sy - Cy;
        r.r0 = sqrtf(dx * dx + dy * dy);
        r.th0 = atan2f(dy, dx);
    }

    // 按 r0 升序错峰：越靠内的图标越早启动，形成级联。
    std::vector<int> order((size_t)s_icons.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [](int a, int b) {
        return s_icons[(size_t)a].r0 < s_icons[(size_t)b].r0;
    });
    for (size_t k = 0; k < order.size(); k++) {
        IconRec& r = s_icons[(size_t)order[k]];
        r.t0 = (float)k * s_stagger;
        r.inited = true;
    }
    s_originInit = true;
}

// --------------------------------------------------------- 动态发现新图标 ---
void DesktopIcons_DiscoverNew() {
    if (!s_attached || !s_devouring) return;
    if (!s_lv || !IsWindow(s_lv)) return;

    int count = QueryItemCount();
    if (count < 0) return;
    if (count == s_lastCount) return;

    if (count > MAX_ICONS) count = MAX_ICONS;
    int oldCount = (int)s_icons.size();
    s_lastCount = count;

    // 消失的索引 → 从队列移除。Listview 索引会因增删而平移，此处采用"近似"处理
    // （本功能为视觉特效，允许近似；精确身份追踪见计划书 §7 的已知限制）。
    if (count < oldCount) {
        s_icons.resize((size_t)count);
    }

    float Cx = s_lastCx, Cy = s_lastCy;
    for (int i = oldCount; i < count; i++) {
        POINT p = { 0, 0 };
        if (!ReadItemPosition(i, &p)) continue;
        IconRec r;
        r.index = i;
        r.sx = p.x;
        r.sy = p.y;
        r.lastX = p.x;
        r.lastY = p.y;
        float dx = (float)p.x - Cx;
        float dy = (float)p.y - Cy;
        r.r0 = sqrtf(dx * dx + dy * dy);
        r.th0 = atan2f(dy, dx);
        r.t0 = s_elapsed; // 新图标从当前位置立即开始吸
        r.inited = true;
        s_icons.push_back(r);
    }
}

// ------------------------------------------------------------------- Tick ---
void DesktopIcons_Update(float holeScreenX, float holeScreenY, float dt) {
    if (!s_devouring) return;
    if (dt <= 0.0f) dt = 0.0f;
    s_elapsed += dt;

    if (!s_lv || !IsWindow(s_lv)) {
        // explorer 崩溃/重启：句柄失效，释放并静默停止，等用户再次触发。
        SetError("桌面 ListView 已失效（explorer 重启？）");
        s_devouring = false;
        DesktopIcons_Detach();
        return;
    }

    if (!s_originInit) InitOriginForAll(holeScreenX, holeScreenY);

    // 周期性发现新图标
    s_discoverAccum += dt;
    if (s_discoverAccum >= DISCOVER_INTERVAL) {
        s_discoverAccum = 0.0f;
        DesktopIcons_DiscoverNew();
    }

    // 限频下发
    s_sendAccum += dt;
    int hz = s_throttleHz > 0 ? s_throttleHz : 30;
    float interval = 1.0f / (float)hz;
    if (s_sendAccum < interval) return;
    s_sendAccum = 0.0f;

    // 黑洞中心 → ListView 客户区坐标（多显示器必须减去客户区原点）。
    float Cx = holeScreenX - (float)s_lvOrigin.x;
    float Cy = holeScreenY - (float)s_lvOrigin.y;
    s_lastCx = Cx;
    s_lastCy = Cy;

    int processed = 0;
    for (size_t i = 0; i < s_icons.size(); i++) {
        IconRec& r = s_icons[i];
        if (!r.active || !r.inited) continue;

        float p = s_elapsed - r.t0;
        if (p < 0.0f) continue; // 错峰未到

        // 螺旋方程（计划书 §5.1）：半径指数内缩，角速度随内缩递增 → 涡旋感。
        float radius = r.r0 * expf(-s_rate * p);
        float th     = r.th0 + s_spin * powf(p, 1.5f);

        if (radius < s_rHide) {
            // 【T6】进入视界 = 图标被吞：立即移到屏幕外，不留残影。
            // Why: 旧实现只置 active=false 停止移动，图标实体滞留在黑洞中心坐标，
            //      黑洞移走后路径上留下一堆堆叠图标。移到 (-16000,-16000) 是 Win32
            //      ListView 的标准"藏图标"手法；Restore 用的是快照 sx/sy 原位，
            //      与 lastX/lastY 无关，恢复功能不受影响。
            WriteItemPosition(r.index, -16000, -16000);
            r.lastX = -16000;
            r.lastY = -16000;
            r.active = false;
            continue;
        }

        int ix = (int)lroundf(Cx + radius * cosf(th));
        int iy = (int)lroundf(Cy + radius * sinf(th));
        if (abs(ix - r.lastX) < 1 && abs(iy - r.lastY) < 1) continue; // <1px 跳过

        if (WriteItemPosition(r.index, ix, iy)) {
            r.lastX = ix;
            r.lastY = iy;
        }
        if (++processed >= MAX_ICONS_PER_TICK) break;
    }
}

// --------------------------------------------------------------- Toggle -----
bool DesktopIcons_Toggle() {
    if (s_devouring) {
        if (s_restore) DesktopIcons_Restore();
        s_devouring = false;
        s_originInit = false;
        DesktopIcons_Detach();
        return true;
    }
    if (!DesktopIcons_Attach()) return false;
    if (!DesktopIcons_Snapshot()) {
        DesktopIcons_Detach();
        return false;
    }
    s_devouring = true;
    s_elapsed = 0.0f;
    s_sendAccum = 0.0f;
    s_discoverAccum = 0.0f;
    s_originInit = false;
    SetError("OK");
    return true;
}

// -------------------------------------------------------------- Shutdown ----
void DesktopIcons_Shutdown() {
    // 退出/崩溃路径：只要还有快照就尝试还原（Q1：退出也还原）。
    if (!s_icons.empty()) {
        DesktopIcons_Restore();
    }
    s_devouring = false;
    DesktopIcons_Detach();
}

bool DesktopIcons_IsDevouring() { return s_devouring; }

const char* DesktopIcons_LastError() { return s_lastError; }

void DesktopIcons_SetParams(float rate, float spin, float stagger, int throttleHz, bool restore) {
    s_rate       = rate;
    s_spin       = spin;
    s_stagger    = stagger;
    s_throttleHz = throttleHz;
    s_restore    = restore;
}

void DesktopIcons_SetRHide(float radiusPx) { s_rHide = radiusPx; }
