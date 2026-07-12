#pragma once

#include <windows.h>

struct MonitorInfo {
    HMONITOR hmon = nullptr;
    int x = 0, y = 0;
    int width = 0, height = 0;
    bool isPrimary = false;
    wchar_t name[32] = {};
};

// Get all connected monitors
int Monitors_Enum(MonitorInfo* out, int maxCount);

// Get primary monitor
HMONITOR Monitors_GetPrimary();

// Get monitor by index (0=primary, 1+=secondary)
HMONITOR Monitors_GetByIndex(int index);
