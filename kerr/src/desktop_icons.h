#pragma once

#include <windows.h>

// 桌面图标漩涡吞噬子系统（子系统 A：真实坐标移动）。
//
// Why：SysListView32 由 explorer.exe 绘制，只能通过 LVM_* 消息改变图标位置，
// 无法缩放/旋转/改变 Z 序；"拉伸成线"由 shader 子系统 B 负责（见 blackhole.glsl）。
// 本模块仅负责把图标沿螺旋轨迹移动到黑洞中心并被覆盖层遮蔽。
//
// 线程约束：所有接口都只在主线程（渲染线程）调用，内部用文件内 static 单例，
// 不做额外加锁。

// 定位桌面 SysListView32。成功返回 true。
bool  DesktopIcons_Attach();
// 释放句柄、恢复被临时改动的样式，不改动图标位置。
void  DesktopIcons_Detach();
bool  DesktopIcons_IsAttached();

// 记录当前所有图标位置（跨进程读，需要内存编组）。
bool  DesktopIcons_Snapshot();
// 把图标写回快照位置；若当前未附加会尝试临时附加后写回。
bool  DesktopIcons_Restore();

// 开始/停止吞噬（内部含 Attach+Snapshot / Restore+Detach）。
bool  DesktopIcons_Toggle();

// 每帧调用：holeScreenX/Y 为黑洞中心在屏幕（物理像素）坐标，dt 为秒。
void  DesktopIcons_Update(float holeScreenX, float holeScreenY, float dt);

// 动态发现新图标（数量变化时重读）。通常由 Update 内部按间隔调用。
void  DesktopIcons_DiscoverNew();

// 退出/异常统一还原：还原快照并释放。可安全重复调用。
void  DesktopIcons_Shutdown();

bool  DesktopIcons_IsDevouring();
const char* DesktopIcons_LastError();

// 由 main.cpp 传入的调参项（每帧或参数变化时调用）。
void  DesktopIcons_SetParams(float rate, float spin, float stagger, int throttleHz, bool restore);

// 视界遮蔽半径（像素）。图标进入该半径即停止移动，由覆盖层吞没。
// TODO: 目前由 main.cpp 用 holeRadius 粗略换算，后续可接入更精确的视界半径。
void  DesktopIcons_SetRHide(float radiusPx);
