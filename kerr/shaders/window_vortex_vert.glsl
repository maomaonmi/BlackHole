#version 460 core

// 窗口吞噬碎片：全屏 quad → 窗口矩形（局部像素空间，y 向下）
// 坐标约定（与 window_devour.cpp 一致）：
//   uCenter/uHole = GL 窗口局部像素（Win32 原点左上，y 向下）
//   uScreen       = viewport 尺寸（像素）

layout(location = 0) in vec2 aPos;   // -1..1 quad

uniform vec2  uCenter;    // 碎片中心（像素）
uniform vec2  uHalfSize;  // 半宽高（像素，含收缩 scale）
uniform float uRot;       // 自转角（弧度）
uniform vec2  uScreen;    // viewport 尺寸

out vec2 vPx;             // 片元像素位置（供 frag 做极点扭曲）

void main() {
    float c = cos(uRot), s = sin(uRot);
    vec2 local   = aPos * uHalfSize;
    vec2 rotated = vec2(local.x * c - local.y * s,
                        local.x * s + local.y * c);
    vec2 px = uCenter + rotated;
    vPx = px;

    // Win32 像素（y 向下）→ NDC（y 向上）：翻转 y
    vec2 ndc = vec2(px.x / uScreen.x * 2.0 - 1.0,
                    1.0 - px.y / uScreen.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
