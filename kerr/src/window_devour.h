#pragma once
#include <windows.h>

// ================================================================
// 窗口吞噬（计划书：窗口吞噬与图标消失-计划书.md F1）
// 中键触发：枚举屏幕可见顶层窗口 → PrintWindow 抓帧 → 统一 SW_HIDE
// → overlay 绘制纹理副本 + 螺旋扭曲收缩动画 → 进入视界后删除纹理。
//
// 自包含模块（仿 gl_texture.cpp 的动态 GL 加载模式），不依赖 main.cpp
// 的 static 函数指针。main.cpp 只需：Init / SetParams / SetRHide /
// Trigger（中键）/ Update（主循环）/ Shutdown（退出）。
// ================================================================

// exe 启动时调用一次：加载 GL、编译 shader、建全屏 quad。
// selfHwnd = GL 覆盖层窗口句柄（枚举时排除自身）。
// 返回 false 时模块静默禁用（Trigger 仍可纯 SW_HIDE，无动画）。
bool  WindowDevour_Init(HWND selfHwnd);

// 每帧调用（黑洞屏幕坐标块附近）：与 iconDevour 同名参数语义一致。
void  WindowDevour_SetParams(float rate, float spin, float warp, float stagger);

// 视界半径（像素）：碎片收缩到该半径即删除纹理（黑洞阴影内部，天然遮挡）。
void  WindowDevour_SetRHide(float radiusPx);

// 中键触发：枚举 + 抓帧 + 统一 SW_HIDE。返回本次吞到的窗口数
// （含黑帧跳过动画的）。无可吞目标返回 0。
// 追加语义：已有动画进行中再触发，新窗口以当前时间为基准错峰入场。
int   WindowDevour_Trigger(float holeScreenX, float holeScreenY);

// 主循环每帧调用（黑洞场景之后、ImGui 之前）：CPU 螺旋状态机 + 渲染。
void  WindowDevour_Update(float holeScreenX, float holeScreenY, float dt);

// 是否有活跃动画记录（无则 main 可跳过渲染态切换）。
bool  WindowDevour_HasActive();

// 退出清理：释放全部纹理 / buffer / program。
void  WindowDevour_Shutdown();
