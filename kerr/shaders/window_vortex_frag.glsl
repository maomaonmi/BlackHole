#version 460 core

// 窗口吞噬碎片：以黑洞为极点的螺旋相位扭曲
// 像素离洞越近 → 相位角越大（1/r 衰减）→ 内容被"拧"成螺旋，
// 配合顶点级收缩/公转，形成被吸入时无限扭曲的观感。

in vec2 vPx;              // 片元像素位置（GL 窗口局部，y 向下）

out vec4 fragColor;

uniform sampler2D uTex;
uniform vec2  uCenter;    // 碎片中心（像素）
uniform vec2  uHalfSize;  // 半宽高（像素）
uniform float uRot;       // 自转角（弧度，与 vert 同值）
uniform vec2  uHole;      // 黑洞中心（像素）
uniform float uWarp;      // 扭曲强度（p^1.5 · warp 增益）

void main() {
    // 1) 逆转自转：像素位置 → quad 局部坐标（内容坐标系）
    float c = cos(uRot), s = sin(uRot);   // R(-uRot) = [c, s; -s, c]
    vec2 rel  = vPx - uCenter;
    vec2 local = vec2(rel.x * c + rel.y * s,
                      -rel.x * s + rel.y * c);

    // 2) 洞投影到 quad 局部系（扭曲极点跟随黑洞实际位置）
    vec2 hRel  = uHole - uCenter;
    vec2 holeLocal = vec2(hRel.x * c + hRel.y * s,
                          -hRel.x * s + hRel.y * c);

    // 3) 螺旋相位扭曲：离洞越近拧得越狠
    vec2 d = local - holeLocal;
    float r = length(d) + 1e-3;
    float ang = uWarp * 120.0 / (r + 60.0);
    float ca = cos(ang), sa = sin(ang);
    vec2 dWarp = vec2(d.x * ca - d.y * sa,
                      d.x * sa + d.y * ca);
    vec2 localWarp = holeLocal + dWarp;

    // 4) 局部坐标 → UV（y 翻转：bottom-up DIB 的 v=0 是图像底部，
    //    而 local.y=+half 是屏幕下方）
    vec2 uv = vec2(localWarp.x / (2.0 * uHalfSize.x) + 0.5,
                   0.5 - localWarp.y / (2.0 * uHalfSize.y));

    vec4 tex = texture(uTex, clamp(uv, 0.0, 1.0));
    // Why 强制不透明：PrintWindow 的 alpha 通道不可靠（GDI 窗口常为全 0），
    // 若按采样 alpha 混合，整窗会直接不可见；圆角处露黑边是小代价。
    fragColor = vec4(tex.rgb, 1.0);
}
