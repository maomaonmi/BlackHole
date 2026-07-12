#include "monitors.h"
#include <cstdio>

struct EnumCtx {
    MonitorInfo* out;
    int maxCount;
    int count;
};

static BOOL CALLBACK MonitorEnumProc(HMONITOR hmon, HDC, LPRECT rect, LPARAM lp) {
    EnumCtx* ctx = (EnumCtx*)lp;
    if (ctx->count >= ctx->maxCount) return FALSE;

    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hmon, &mi);

    MonitorInfo& info = ctx->out[ctx->count];
    info.hmon = hmon;
    info.x = mi.rcMonitor.left;
    info.y = mi.rcMonitor.top;
    info.width = mi.rcMonitor.right - mi.rcMonitor.left;
    info.height = mi.rcMonitor.bottom - mi.rcMonitor.top;
    info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    wcsncpy(info.name, mi.szDevice, 31);
    info.name[31] = L'\0';

    ctx->count++;
    return TRUE;
}

int Monitors_Enum(MonitorInfo* out, int maxCount) {
    EnumCtx ctx = { out, maxCount, 0 };
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&ctx);
    return ctx.count;
}

HMONITOR Monitors_GetPrimary() {
    return MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
}

HMONITOR Monitors_GetByIndex(int index) {
    if (index <= 0) return Monitors_GetPrimary();

    MonitorInfo monitors[8] = {};
    int count = Monitors_Enum(monitors, 8);

    // Primary first, then secondaries
    HMONITOR primary = Monitors_GetPrimary();
    int secondaryIdx = 0;
    for (int i = 0; i < count; i++) {
        if (monitors[i].hmon == primary) continue;
        secondaryIdx++;
        if (secondaryIdx == index) return monitors[i].hmon;
    }
    return primary;
}
