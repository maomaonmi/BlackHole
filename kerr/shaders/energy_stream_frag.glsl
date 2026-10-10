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
// 潮汐模式：>= 0 时拉长/盘绕进度由 CPU 统一积分的剥离进度 p 驱动（<0 → 退回 age 定时器路径）
uniform float uTidalProgress     = -1.0;
// 潮汐形变幅度：与 star_frag.glsl 使用同一数值（S4 不变量），0 = 未撕裂
uniform float uTidalAmp          = 0.0;
// 剥离物质流强度 strip = p^1.2（双量分离的"尾部主导"量）：只控制流的宽度/亮度/色温。
// Why: 若用 tidalAmp 驱动流，则流在 p 较小时就已经很粗，撕裂"过程"被压缩成一次闪现。
//      strip 是 p 的超线性函数，使流在撕裂后半程才爆发 → 过程感明显。
uniform float uTidalStrip        = 0.0;
// 本体真实半径（UV, aspect 校正）。>0 时用它与 uTidalAmp 精确对齐流起点，替代原有启发式
uniform float uStarSize          = 0.0;
// 真实吸积盘内/外径（aspect 校正 UV）。由 CPU 用 blackhole.glsl 的权威 rhT 换算后广播。
// Why: 本 shader 曾自行重复推导 DEMO_SEC/DEMO_GROW_SEC/rh，与 blackhole.glsl 用途不同却公式相似，
//      导致螺旋半径比真实盘大 2.0~2.6 倍，末端悬在盘外并被裁掉（观感="被黑洞外围引力场挡住"）。
//      缺省值对应默认档 (rh≈0.133)：inner=1.8/2.598*0.133≈0.092，outer=8/2.598*0.133≈0.41。
uniform float uDiskInnerUV       = 0.092;
uniform float uDiskOuterUV       = 0.410;

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

    float tdeProgress;
    float spiralProgress;
    if (uTidalProgress >= 0.0) {
        // 潮汐模式：进度与本体形变严格同源（p 已同时包含距离与时间两个因素）
        // 【F3/G2-B 实机反馈修正】两轮教训：
        //   旧式 max(p,0.02) → 6px 残桩（几何闸门漏修）；改 0.30+0.70p 后用户仍反馈
        //   "流到中途就停，像被挡住"。两个原因：① 贝塞尔参数 s≠弧长，s=0.30 的实际
        //   可见路程只有约 21%（B(0.3) 权重），底噪仍太短；② p<0.55 时螺旋段整段
        //   不画，流终点悬在半空，与黑洞之间永远留真空隙。
        // 现改为：底噪 0.60（可见路程 ≈52%），螺旋段从 p=0.55 提前介入且保留 0.10
        //   底噪 —— 两端同时可见，随 p 相向生长，p=1 时无缝汇合。
        tdeProgress    = clamp(0.60 + 0.40 * max(uTidalProgress, 0.0), 0.02, 1.0);
        spiralProgress = clamp(0.10 + 0.90 * max(uTidalProgress - 0.55, 0.0) / 0.45, 0.0, 1.0);
    } else {
        // 回退：原有 age 定时器（白矮星 / 中子星 / 黑洞桥 / 关闭潮汐开关时）
        // 前 5.0 秒为能量流向外拉长伸展的阶段
        tdeProgress = clamp(age / 5.0, 0.001, 1.0);
        // 5.0 秒后完全抵达吸积盘外沿，在接下来的 4 秒内缓慢盘旋缠绕
        float spiralAge = max(0.0, age - 5.0);
        spiralProgress = clamp(spiralAge / 4.0, 0.0, 1.0);
    }

    // ------------------------------------------------------------
    // 2. 吸积区半径：来自 CPU 广播的真实盘半径（T1/D4）
    // ------------------------------------------------------------
    // 不再自行推导 rh：本 shader 无法知道 supermassiveMode / 用户覆盖等 CPU 侧状态，
    // 一旦自行推导就必然与 blackhole.glsl 漂移。这里只做退化保护。
    float diskInner = max(uDiskInnerUV, 0.01);
    float diskOuter = max(uDiskOuterUV, diskInner + 0.02);

    // ------------------------------------------------------------
    // 3. 三维椭圆投影 (DISK_INCL = 1.5, DISK_ROLL = 0.35)
    // ------------------------------------------------------------

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
    // 【S4 不变量】流的起点必须落在本体被拉伸后的鼻尖上。
    // 与 star_frag.glsl 使用同一个 uTidalAmp，因此两侧不可能各自漂移。
    float tidalAmp = max(uTidalAmp, 0.0);
    // strip=0 时下面所有 mix/乘法都必须退回原值（G9：关闭开关完全等价改动前）
    float strip = clamp(uTidalStrip, 0.0, 1.0);
    float starRadius;
    float noseWidth;
    // 【F3b】只要知道本体半径就走 D5 精确对齐（amp=0 时 L_nose=R 恰为未形变鼻尖）。
    // Why: 旧条件 tidalAmp>0 使 p=0 时退回启发式 starRadius=0.18，与真实半径
    //      （uStarSize 可为 0.25）错位 —— 底噪细丝会从"空气"里出发，而不是从鼻尖出发。
    if (uStarSize > 0.0) {
        // 【S4 交界不变量 / D5】与 star_frag.glsl 共用同一组解析式（不再各自用魔数）：
        //   L_nose(amp) = R·(1 + amp)              本体沿潮汐轴的可见半长
        //   W_nose(amp) = R·max(1 - 0.30·amp, 0.35) 本体在鼻尖处的可见半宽
        // Why: 旧式 R/(1+amp·0.55) 与本体 R·(1-0.30·amp) 在 amp=0.6 时相差 9%，
        //      两侧改一处忘一处 → 流起点与本体鼻尖立刻错位。现在同一个 amp 只会得到同一个值。
        starRadius = uStarSize * (1.0 + tidalAmp);
        noseWidth  = max(uStarSize * max(1.0 - 0.30 * tidalAmp, 0.35), 0.004);
    } else {
        starRadius = min(0.18, starToBH * 0.25);
        noseWidth  = 0.04;
    }
    vec2 p0 = star_corr + dirBH * starRadius; 
    
    float shootOutDist = max(starToBH * 0.20, 0.06);
    vec2 p1 = p0 + dirBH * shootOutDist;

    // 【调节速度点】：重力空中微幅缓慢摇摆频数 (0.08 越小越慢)
    // 用双频正弦取代原 sign() 方波：方波每半周期产生一次可见跳变
    float arcSide = sin(uTime * 0.08) + 0.35 * sin(uTime * 0.017);
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

        // S4-b / T6：尾端宽度随 strip 增粗，但收缩得更快（sqrt(s)）。
        // Why: 加色混合下贡献 ∝ 颜色×alpha，与宽度无关 —— "宽而暗"在亮盘面上没有信噪比，
        //      只有"细而亮"的丝才能被看见。Step A 的 0.085 均匀尾宽正是"流不明显"的成因之一。
        float wBase = mix(noseWidth, mix(0.010, 0.042, strip), sqrt(s)) * lengthMask; 
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

            // S4-c：螺旋段宽度同样随 strip 增粗（strip=0 → 原式）
            float sw = mix(0.015, 0.006, totalProg) * uStreamIntensity * uStreamWidth
                     * (1.0 + 1.5 * strip);

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

    // 【T7 材质一体化 / D5 同源】起点色 = 本体光球的最热色，显式取自 star_frag.glsl 的同一常量
    //   star_frag.glsl: coreCol = mix(uStarColor, vec3(1.0), 0.85)
    // Why: 旧式 mix(vec3(1,0.98,0.95), starTint, 0.85) 是经验系数，与本体无解析关系 ——
    //      这正是"能量流和恒星本体根本不搭边"的颜色层成因。现在同一 amp/同一 tint 只会得到同一个色。
    vec3 bodyHotCol = mix(starTint, vec3(1.0), 0.85);
    vec3 colStarEnd = mix(bodyHotCol, starTint, 0.35); // 起点偏白热，与本体最热色连续
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

    // S4-e：剥离越深 → 物质被强引力/激波加热 → 色温向蓝白偏移（真实 TDE 的 UV/X 射线特征）。
    // strip=0 时 mix 系数为 0，颜色与改动前逐位等价。
    streamCol = mix(streamCol, vec3(0.72, 0.84, 1.0), strip * 0.55);

    // 【T6 白芯】阈值 0.08 → 0.05（更细），融合比例 0.1 → 0.35（更亮）。
    // Why: 加色混合下白芯是唯一能在亮盘面上"赢"出来的高频亮点；旧值让流只剩一层暗雾。
    float core = smoothstep(0.05, 0.0, bestDist);
    vec3 coreColor = mix(vec3(1.0), starTint, 0.6) * 1.1;
    streamCol = mix(streamCol, coreColor, core * 0.35);

    // 边缘气态火晕
    float edgeGlow = smoothstep(1.5, 0.3, bestDist) * (1.0 - smoothstep(0.3, 0.0, bestDist));
    vec3 edgeColor = mix(colBHEnd, starTint, 0.6);
    streamCol += edgeColor * edgeGlow * 0.6;

    // 【关键发光点 1：全局亮度总控】：
    // 你可以直接把 1.6 往更低调（如 0.8, 0.5, 0.3 等），整体流体就会非常温润自然，完全不刺眼！
    // S4-d：strip 越大整体越亮（strip=0 → ×0.5，与改动前逐位等价）
    streamCol *= uStreamIntensity * 0.5 * (1.0 + 2.0 * strip);

    float alpha = clamp(mask * clamp(uStreamIntensity * 1.5, 0.0, 1.0), 0.0, 1.0);

    if (alpha < 0.003) discard;

    fragColor = vec4(streamCol, alpha);
}