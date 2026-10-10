#pragma once

#include <windows.h>
// Do NOT include gl/gl.h here - it declares gl* functions that conflict
// with our dynamic loading declarations in main.cpp

struct Win32GL {
    HWND  hwnd = nullptr;
    HDC   hdc  = nullptr;
    HGLRC hglrc = nullptr;
    int   width  = 0;
    int   height = 0;
    bool  active = true;
    bool  shouldClose = false;
    HMONITOR hmon = nullptr;
    LARGE_INTEGER freq = {};
    LARGE_INTEGER start = {};
};

// Window + WGL context
bool Win32GL_Init(Win32GL* wgl, HMONITOR hmon, int w, int h);
void Win32GL_Shutdown(Win32GL* wgl);
void Win32GL_Show(Win32GL* wgl);
void Win32GL_Hide(Win32GL* wgl);
void Win32GL_SwapBuffers(Win32GL* wgl);
bool Win32GL_PollEvents(Win32GL* wgl);
double Win32GL_GetTime(Win32GL* wgl);
void Win32GL_EnableLayered(Win32GL* wgl);
void Win32GL_HideSystemCursor();
void Win32GL_RestoreSystemCursor();
void Win32GL_SetClickThrough(Win32GL* wgl, bool enabled);

// WGL extension loader
PROC Win32GL_GetProcAddress(const char* name);
