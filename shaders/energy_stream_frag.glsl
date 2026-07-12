#version 460 core

in vec2 vUV;
out vec4 fragColor;

// Energy stream uniforms with safe fallback defaults
uniform vec2  uStarPos          = vec2(0.5, 0.5);          
uniform vec2  uBHPos            = vec2(0.96, 0.04);        
uniform float uStreamIntensity  = 1.0;                     
uniform float uTime             = 0.0;
uniform vec2  uResolution       = vec2(2560.0, 1440.0);    
uniform float uStreamWidth       = 1.0;                     
uniform float uSpiralTurns       = 2.0;                     
uniform vec3  uStreamColor       = vec3(1.0, 0.8, 0.5);     
uniform float uStarAge           = -1.0; // Dynamic star age sent from CPU (-1.0 = fallback auto-cycle)

// ================================================================
// Noise toolkit (strictly typed for maximum compatibility)
// ================================================================
float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + vec2(34.23)); 
    return fract(p.x * p.y);
}

float hash11(float n) {
    return fract(sin(n) * 43758.5453123);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x), 
        mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x), f.y);
}

float fbm(vec2 p, int octaves) {
    float v = 0.0, a = 0.5;
    mat2 rot = mat2(0.8, 0.6, -0.6, 0.8);
    for (int i = 0; i < octaves; i++) {
        v += a * valueNoise(p);
        p = rot * p * 2.0;
        a *= 0.5;
    }
    return v;
}

float turbulent(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 8; i++) {
        vec2 q = p * (1.0 + float(i));
        v += a * (sin(q.x + valueNoise(q)) + cos(q.y + valueNoise(q + vec2(10.0))));
        a *= 0.55;
    }
    return v;
}

// Cubic Bezier curve evaluation (Cubic Bezier)
vec2 bezier3(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t) {
    float mt = 1.0 - t;
    return mt*mt*mt*p0 + 3.0*mt*mt*t*p1 + 3.0*mt*t*t*p2 + t*t*t*p3;
}

void main() {
    if (uStreamIntensity < 0.01) discard;

    // Guard against zero division in aspect calculation
    float resY = uResolution.y > 1.0 ? uResolution.y : 1080.0;
    float resX = uResolution.x > 1.0 ? uResolution.x : 1920.0;
    float aspect = resX / resY;

    // Convert all inputs to aspect-corrected space
    vec2 uv_corr    = vec2(vUV.x * aspect, vUV.y);
    vec2 star_corr  = vec2(uStarPos.x * aspect, uStarPos.y);
    vec2 bh_corr    = vec2(uBHPos.x * aspect, uBHPos.y);

    vec2 bhDelta = bh_corr - star_corr;
    float starToBH = length(bhDelta);

    // Direction from star to BH
    vec2 dirBH = normalize(bhDelta);
    vec2 perpDir = vec2(-dirBH.y, dirBH.x);

    // ------------------------------------------------------------
    // 1. 潮汐撕裂 (TDE) 渐进时间控制 (拉扯与盘绕进度的平滑演化)
    // ------------------------------------------------------------
    float age = uStarAge >= 0.0 ? uStarAge : mod(uTime, 12.0);
    
    // 前 5.0 秒为能量流向外拉长伸展的阶段
    float tdeProgress = clamp(age / 5.0, 0.001, 1.0);
    
    // 5.0 秒后完全抵达吸积盘外沿，在接下来的 4 秒内缓慢盘旋缠绕
    float spiralAge = max(0.0, age - 5.0);
    float spiralProgress = clamp(spiralAge / 4.0, 0.0, 1.0);

    // ------------------------------------------------------------
    // 2. 解算黑洞动态视界半径 rh
    // ------------------------------------------------------------
    float DEMO_SEC = 42.0;
    float DEMO_GROW_SEC = 40.0;
    float lvl = min(mod(uTime, DEMO_SEC) / DEMO_GROW_SEC, 1.0);
    float gSize = pow(clamp(lvl, 0.0, 1.0), 1.0); 
    float rhMin = sqrt(0.0100 * aspect / 3.14159265); 
    float rhMax = sqrt(0.5000 * aspect / 3.14159265); 
    float rh = mix(rhMin, rhMax, gSize) * (0.0200 / 0.08); 

    // ------------------------------------------------------------
    // 3. 吸积区与三维椭圆投影 (DISK_INCL = 1.5, DISK_ROLL = 0.35)
    // ------------------------------------------------------------
    float diskOuter = min(6.0 * rh, starToBH * 0.7);
    float diskInner = min(1.8 * rh, diskOuter * 0.3);

    float incl = 1.5;   
    float roll = 0.35;  
    float cosI = cos(incl);
    float cosR = cos(roll);
    float sinR = sin(roll);

    // 【调节速度点】：极慢而庄严的吸积角速度（降至 -uTime * 0.1 呈缓慢宏伟的天体坠落）
    float angle_start = -uTime * 0.1; 
    
    vec2 offset_start = vec2(cos(angle_start), sin(angle_start)) * diskOuter;
    vec2 projOffset_start;
    projOffset_start.x = (offset_start.x * cosR - offset_start.y * cosI * sinR);
    projOffset_start.y = (offset_start.x * sinR + offset_start.y * cosI * cosR);
    vec2 spiral_start = bh_corr + projOffset_start;

    // ------------------------------------------------------------
    // 4. 三次贝塞尔曲线控制点 (Cubic Bezier)
    // ------------------------------------------------------------
    float starRadius = min(0.18, starToBH * 0.25);
    vec2 p0 = star_corr + dirBH * starRadius; 
    
    float shootOutDist = max(starToBH * 0.20, 0.06);
    vec2 p1 = p0 + dirBH * shootOutDist;

    // 【调节速度点】：重力空中微幅缓慢摇摆频数 (0.08 越小越慢)
    float arcSide = sign(sin(uTime * 0.08)); 
    float arcAmount = starToBH * 0.22; 
    vec2 p2 = (p1 + spiral_start) * 0.5 + perpDir * arcAmount * arcSide;
    vec2 p3 = spiral_start;

    const int BEZIER_SAMPLES = 80;

    float bestDist = 9999.0;
    float bestProgress = 0.0;
    float bestWidth = 0.0;
    bool isInSpiral = false;

    // 采样段 1：三次贝塞尔流体通道 (恒星表面 -> 无缝桥接至吸积圈)
    for (int i = 0; i <= BEZIER_SAMPLES; i++) {
        float s = float(i) / float(BEZIER_SAMPLES);
        
        // 尖头拉伸控制：未达到的部分直接剪裁，并在尖端形成尖头形状
        if (s > tdeProgress) break; 
        
        vec2 pt = bezier3(p0, p1, p2, p3, s);
        float dist = length(uv_corr - pt);

        // 尖头平滑收缩
        float lengthMask = smoothstep(tdeProgress, max(0.001, tdeProgress - 0.15), s);

        float wBase = mix(0.04, 0.015, s) * lengthMask; 
        wBase *= (0.8 + 0.5 * hash11(s * 100.0)); 
        wBase *= uStreamWidth; 
        float w = wBase * uStreamIntensity;

        float dRatio = dist / w;
        if (dRatio < bestDist) {
            bestDist = dRatio;
            bestProgress = s;
            bestWidth = w;
            isInSpiral = false;
        }
    }

    // ------------------------------------------------------------
    // 关键升级：采样段 2：解析式极坐标解投影算法（3D Analytical Plane Projection）
    // ------------------------------------------------------------
    // 彻底摒弃 24 点离散点循环，将像素反向解投影，获得完美的连续平滑流体轨道！
    vec2 dp = uv_corr - bh_corr;

    // 1. 反向旋转
    float unRot_x = dp.x * cosR + dp.y * sinR;
    float unRot_y = -dp.x * sinR + dp.y * cosR;

    // 2. 反向拉伸（解扁平化）
    vec2 flatOffset = vec2(unRot_x, unRot_y / cosI);
    float r_pixel = length(flatOffset);
    float theta_pixel = atan(flatOffset.y, flatOffset.x);

    int maxTurns = int(clamp(uSpiralTurns, 1.0, 4.0));
    float totalAngleSpan = float(maxTurns) * 2.0 * 3.14159265;

    // 解析式判定像素到连续螺旋轨道的欧式距离
    for (int k = -1; k <= 4; k++) {
        // 解算在该角度上，第 k 圈轨道的螺旋对应角度
        float relative_angle = angle_start - theta_pixel + float(k) * 2.0 * 3.14159265;
        
        // 检查该轨道是否处于合法的螺旋跨度内
        if (relative_angle >= 0.0 && relative_angle <= totalAngleSpan) {
            float totalProg = relative_angle / totalAngleSpan;
            
            // 渐进吸积螺旋判定
            if (totalProg > spiralProgress) continue;

            float r_spiral = mix(diskOuter, diskInner, totalProg);
            // 径向平滑波动
            r_spiral *= (0.85 + 0.3 * sin(totalProg * 20.0 + uTime * 2.0)); 

            // 计算平坦盘面上的欧式测距
            float d_flat = abs(r_pixel - r_spiral);

            // 投影回屏幕空间，以保证流体在长宽比下拉伸宽度仍然是一致的
            float scale = mix(1.0, cosI, abs(sin(theta_pixel)));
            float dist = d_flat * scale;

            float sw = mix(0.015, 0.006, totalProg) * uStreamIntensity * uStreamWidth;

            float dRatio = dist / sw;
            if (dRatio < bestDist) {
                bestDist = dRatio;
                bestProgress = 1.0 + totalProg; 
                bestWidth = sw;
                isInSpiral = true;
            }
        }
    }

    if (bestDist > 1.5) discard;

    // ============================================================
    // 连续流体纹理解算（100% 无颗粒层流流动）
    // ============================================================
    float mask = smoothstep(1.5, 0.0, bestDist);
    
    // 【调节速度点】：气体流动推进速度（从 0.8/1.5 调慢至 0.3/0.5 呈非常宏伟、慢速的推进）
    vec2 flowUV = vec2(bestProgress * 1.5 - uTime * 0.3, bestDist * 1.0);
    float streamPlasma = fbm(flowUV, 4);

    vec2 flowUV2 = vec2(bestProgress * 3.0 - uTime * 0.5, bestDist * 2.0);
    float finePlasma = fbm(flowUV2, 3);

    // 混合流体运动
    float structure = mix(streamPlasma, finePlasma, 0.3);
    structure = smoothstep(0.1, 0.9, structure) * 0.75 + 0.25; // 高度连贯的层流气体

    float flicker = (0.85 + 0.15 * sin(uTime * 10.0 + bestProgress * 20.0));

    // ------------------------------------------------------------
    // 调节亮度与光谱色彩（Color Tuning Zone）
    // ------------------------------------------------------------
    vec3 starTint = uStreamColor; 
    
    // 【调节发光点 4】：你可以增大 0.85 到 1.0。越大，起点恒星大气的纯色就越浓，白芯越少
    vec3 colStarEnd = mix(vec3(1.0, 0.98, 0.95), starTint, 0.85); 
    vec3 colMid     = starTint; // 中间段 100% 纯正的恒星光谱本色
    vec3 colBHEnd   = mix(vec3(0.92, 0.25, 0.05), starTint, 0.65); // 捕获端汇入黄色吸积盘

    vec3 streamCol;
    if (!isInSpiral) {
        float progClamped = clamp(bestProgress, 0.0, 1.0);
        if (progClamped < 0.33)
            streamCol = mix(colStarEnd, colMid, clamp(progClamped * 3.0, 0.0, 1.0));
        else
            streamCol = mix(colMid, colBHEnd, clamp((progClamped - 0.33) * 1.5, 0.0, 1.0));
    } else {
        float spiralProg = bestProgress - 1.0;
        streamCol = mix(colMid, colStarEnd, clamp(spiralProg * 0.5, 0.0, 1.0));
        streamCol = mix(streamCol, vec3(1.0, 1.0, 0.95), clamp(spiralProg * spiralProg * 0.4, 0.0, 1.0));
    }

    streamCol *= (structure * flicker);

    // 【调节发光点 3】：白热核心管道粗细。你可以将 0.08 下调到 0.02 使白芯极细
    float core = smoothstep(0.08, 0.0, bestDist);
    vec3 coreColor = mix(vec3(1.0, 1.0, 1.0), starTint, 0.6) * 1.1;
    
    // 【调节发光点 2】：白芯的融合比例。你可以将 0.25 降为 0.0，使整条流体完全不带白色，实现纯原色渲染！
    streamCol = mix(streamCol, coreColor, core * 0.1);

    // 边缘气态火晕
    float edgeGlow = smoothstep(1.5, 0.3, bestDist) * (1.0 - smoothstep(0.3, 0.0, bestDist));
    vec3 edgeColor = mix(colBHEnd, starTint, 0.6);
    streamCol += edgeColor * edgeGlow * 0.6;

    // 【关键发光点 1：全局亮度总控】：
    // 你可以直接把 1.6 往更低调（如 0.8, 0.5, 0.3 等），整体流体就会非常温润自然，完全不刺眼！
    streamCol *= uStreamIntensity * 0.5;

    float alpha = clamp(mask * clamp(uStreamIntensity * 1.5, 0.0, 1.0), 0.0, 1.0);

    if (alpha < 0.003) discard;

    fragColor = vec4(streamCol, alpha);
}