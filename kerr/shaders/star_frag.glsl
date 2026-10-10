#version 460 core

in vec2 vUV;
out vec4 fragColor;

uniform vec2  uStarPos;
uniform float uStarSize;
uniform float uStarAlpha;
uniform vec3  uStarColor;
uniform float uTime;
uniform float uCoronaSize;
uniform float uCoronaBright;
uniform float uFlareChance;
uniform vec2  uResolution;
uniform vec2  uBHPos;
// 潮汐形变幅度：由 CPU 统一积分剥离进度后计算（本体与能量流共用同一数值，S4 不变量）。
// == 0 表示未撕裂（或开关关闭）→ 完全无形变。
// Why: v1.x 曾在这里保留一条"距离型"回退路径（uBHInfluence），它会在开关关闭时用距离
//      驱动形变，与本轮的时间积分语义冲突（双驱动源 = 历史 bug 根因），已删除。
uniform float uTidalAmp;
// 分层体积气体材质（T3~T5）。D1：材质全局常开，不受 uTidalAmp / tidalEnabled 影响。
uniform float uGasOpacity      = 3.2;    // k₁ 光球吸收系数（越大越实、越小越透）
uniform float uChromoScale     = 1.75;   // L2 色球相对 L1 的尺度倍数
uniform float uCoronaFalloff   = 1.10;   // L3 日冕衰减常数（取代旧硬编码 3.0）
uniform float uStreakAmount    = 0.90;   // 拉丝对密度的调制强度（0 = 无拉丝）
uniform float uStreakSpeed     = 3.50;   // 拉丝相速度随 strip 的增益
// 撕裂相关量（关闭 tidalEnabled 时 CPU 传 0 → 拉丝/消散全部关闭，G9）
uniform float uTidalStrip;      // 剥离物质流强度 strip = p^1.2（与 energy_stream_frag.glsl 同源）
uniform float uTidalBodyFade;   // 本体消散强度 [0,1]
uniform float uTidalProgress;   // 剥离进度 p ∈ [0,1]

// Stellar evolution uniforms
uniform int   uStarStage;          // 0 normal, 1 white dwarf, 2 neutron star, 3 black hole
uniform int   uPassMode;           // 0 body (alpha blend), 1 emissive (additive)
uniform float uCollapseProgress;   // 0..1 collapse progress (<0 = inactive)
uniform float uSupernovaProgress;  // 0..1 supernova progress (<0 = inactive)
uniform float uExpandProgress;     // 0..1 first-right-click expansion progress
uniform float uCollapseTension;    // 0..1 tension line intensity (emissive pass)
// White dwarf
uniform float uWDBright;
uniform float uWDCrystal;
// Neutron star
uniform float uNSRotation;
uniform float uNSMagAxisTilt;
uniform float uNSJetLen;
uniform float uNSJetBright;
uniform float uNSPulseRate;
// Mini black hole
uniform float uBHFormationProgress;

// Dual black hole system
uniform sampler2D uFBTexture;          // framebuffer capture (background without stars)
uniform float uMiniBHLensStrength;      // gravitational lensing strength
uniform float uMiniBHRingBright;        // photon ring brightness multiplier
uniform float uDualBHActive;            // 1.0 if dual BH system active, 0.0 otherwise
uniform float uGWRippleStrength;        // gravitational wave ripple intensity
uniform float uAccretionBridgeBright;    // accretion bridge brightness
uniform float uMiniBHDiskIncl;           // mini BH accretion disk inclination (independent, 0=face-on, ~1.5=edge-on)

// ================================================================
// Noise toolkit
// ================================================================
float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float hash11(float n) {
    return fract(sin(n) * 43758.5453);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash21(i), hash21(i + vec2(1, 0)), f.x),
        mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x),
        f.y
    );
}

// 3D value noise for seamless sphere surface
float hash31(vec3 p) {
    p = fract(p * vec3(234.34, 435.345, 178.92));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y * p.z);
}

float valueNoise3D(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(mix(hash31(i), hash31(i + vec3(1,0,0)), f.x),
            mix(hash31(i + vec3(0,1,0)), hash31(i + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash31(i + vec3(0,0,1)), hash31(i + vec3(1,0,1)), f.x),
            mix(hash31(i + vec3(0,1,1)), hash31(i + vec3(1,1,1)), f.x), f.y),
        f.z
    );
}

float fbm3D(vec3 p, int octaves) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < octaves; i++) {
        v += a * valueNoise3D(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

// Seamless turbulent noise using 3D sampling
float turbulentSeamless(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 8; i++) {
        vec3 q = p * (1.0 + float(i));
        v += a * (sin(q.x + valueNoise3D(q)) + cos(q.y + valueNoise3D(q + vec3(10.0))));
        a *= 0.55;
    }
    return v;
}

// Fractal Brownian Motion (2D, for non-spherical use)
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

// Gabor-like turbulent noise (filamentary structure)
float turbulent(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 8; i++) {
        vec2 q = p * (1.0 + float(i));
        v += a * (sin(q.x + valueNoise(q)) + cos(q.y + valueNoise(q + 10.0)));
        a *= 0.55;
    }
    return v;
}

// Voronoi cell noise
float voronoi(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float minDist = 1.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 point = vec2(hash21(i + neighbor), hash21(i + neighbor + vec2(31.0, 17.0)));
            float d = length(neighbor + point - f);
            minDist = min(minDist, d);
        }
    }
    return minDist;
}

// 3D-ish turbulence flow
vec2 warp(vec2 p, float t) {
    float n1 = fbm(p + vec2(t * 0.12, 0.0), 4);
    float n2 = fbm(p + vec2(0.0, t * 0.18) + 5.0, 4);
    return p + vec2(n1, n2) * 0.2;
}

// Rotate a 3D point around an arbitrary axis
vec3 rotate3D(vec3 p, vec3 axis, float angle) {
    axis = normalize(axis);
    float c = cos(angle);
    float s = sin(angle);
    float oc = 1.0 - c;
    return vec3(
        (oc * axis.x * axis.x + c) * p.x + (oc * axis.x * axis.y - axis.z * s) * p.y + (oc * axis.z * axis.x + axis.y * s) * p.z,
        (oc * axis.x * axis.y + axis.z * s) * p.x + (oc * axis.y * axis.y + c) * p.y + (oc * axis.y * axis.z - axis.x * s) * p.z,
        (oc * axis.z * axis.x - axis.y * s) * p.x + (oc * axis.y * axis.z + axis.x * s) * p.y + (oc * axis.z * axis.z + c) * p.z
    );
}

// ================================================================
// Stellar evolution helpers (white dwarf / neutron star / black hole)
// ================================================================

// Ridged voronoi for crystalline cracks
float ridgedVoronoi(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float maxR = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 point = vec2(hash21(i + neighbor), hash21(i + neighbor + vec2(31.0, 17.0)));
            float d = 1.0 - length(neighbor + point - f);
            maxR = max(maxR, d);
        }
    }
    return maxR;
}

// White dwarf body: crystalline carbon/oxygen surface with white glow
vec4 whiteDwarfColor(vec2 d, float nr, float angle, float t, float R) {
    if (nr > uCoronaSize * 1.8) discard;

    vec3 col = vec3(0.0);
    float alpha = 0.0;

    if (nr < 1.0) {
        float r2 = nr * nr;
        float cosTheta = sqrt(max(1.0 - r2, 0.0));
        vec3 n = normalize(vec3(d.x / R, d.y / R, cosTheta));
        // slow rotation
        float rotA = t * 0.03;
        float c = cos(rotA), s = sin(rotA);
        n = vec3(c * n.x - s * n.z, n.y, s * n.x + c * n.z);

        // Crystalline lattice: multi-octave ridged voronoi (carbon/oxygen crystal facets)
        float octaves = clamp(uWDCrystal, 2.0, 10.0);
        vec3 np = n * 6.0;
        float crystal = 0.0;
        float amp = 0.5;
        float freq = 1.0;
        for (int i = 0; i < 10; i++) {
            if (float(i) >= octaves) break;
            crystal += amp * ridgedVoronoi(np.xy * freq + vec2(float(i) * 17.3));
            freq *= 2.1;
            amp *= 0.55;
        }
        crystal = clamp(crystal, 0.0, 1.5);

        // Facet edges: sharp bright grain boundaries
        float facets = smoothstep(0.55, 0.75, crystal);
        float fine = fbm3D(np * 18.0 + vec3(t * 0.02), 4);

        // Hot spots: faint brighter regions (residual heat)
        float hotspots = smoothstep(0.6, 0.9, fbm3D(np * 2.5 + vec3(100.0), 5));

        float structure = crystal * 0.6 + facets * 0.3 + fine * 0.1;
        structure = mix(0.3, 1.3, structure);

        // Color: white-blue core, faint blue tints at facet edges
        vec3 base = vec3(0.92, 0.94, 1.0);          // white
        vec3 edge = vec3(0.7, 0.8, 1.0);            // cool blue
        vec3 hot  = vec3(1.0, 0.98, 0.92);          // warm white hot spot
        col = mix(edge, base, structure);
        col = mix(col, hot, hotspots * 0.4);

        // Limb darkening - subtle (white dwarfs are compact & bright)
        float limbDark = 0.55 + 0.45 * cosTheta;
        col *= limbDark;

        // Brightness boost
        col *= uWDBright;

        // Soft pulse
        float pulse = 1.0 + 0.04 * sin(t * 0.8);
        col *= pulse;

        alpha = 1.0;
    } else {
        // Faint glow halo
        float distFromEdge = nr - 1.0;
        float falloff = exp(-distFromEdge * 4.0);
        float streamers = fbm(vec2(angle * 6.0 + t * 0.02, distFromEdge * 8.0), 4);
        streamers = 0.4 + 0.6 * streamers;
        float g = falloff * streamers;
        col = vec3(0.85, 0.9, 1.0) * g * uWDBright * 0.6;
        alpha = g * 0.5;
    }
    return vec4(col, alpha);
}

// Neutron star body: ultra-dense cracked crust, blue-violet, fast rotation
vec4 neutronStarColor(vec2 d, float nr, float angle, float t, float R) {
    if (nr > uCoronaSize * 1.5) discard;

    vec3 col = vec3(0.0);
    float alpha = 0.0;

    if (nr < 1.0) {
        float r2 = nr * nr;
        float cosTheta = sqrt(max(1.0 - r2, 0.0));
        vec3 n = normalize(vec3(d.x / R, d.y / R, cosTheta));
        // fast rotation
        float rotA = uNSRotation;
        float c = cos(rotA), s = sin(rotA);
        n = vec3(c * n.x - s * n.z, n.y, s * n.x + c * n.z);

        // Cracked crust: very high frequency ridged voronoi (neutron star crust "plates")
        vec3 np = n * 14.0;
        float crust = 0.0;
        float amp = 0.5;
        float freq = 1.0;
        for (int i = 0; i < 8; i++) {
            crust += amp * ridgedVoronoi(np.xy * freq + vec2(float(i) * 23.1));
            freq *= 2.3;
            amp *= 0.5;
        }
        crust = clamp(crust, 0.0, 1.5);

        // Fine micro-fracture network (extremely detailed)
        float micro = fbm3D(np * 40.0 + vec3(t * 0.05), 3);
        float cracks = smoothstep(0.5, 0.62, crust);
        float plates = smoothstep(0.3, 0.5, crust);

        // Glowing magnetic field lines on surface (pulsar surface heating)
        float fieldLines = sin(n.y * 18.0 + uNSRotation * 2.0) * 0.5 + 0.5;
        fieldLines = pow(fieldLines, 6.0);

        // Ultra-hot polar caps (where jets emerge)
        vec2 polarAxis = vec2(cos(uNSMagAxisTilt), sin(uNSMagAxisTilt));
        float polarDot = abs(dot(normalize(d), polarAxis));
        float polarCap = pow(polarDot, 8.0) * smoothstep(1.0, 0.5, nr);

        float structure = crust * 0.45 + plates * 0.25 + micro * 0.15 + cracks * 0.15;

        // Color: deep blue-violet base, hot blue-white ridges, glowing cracks
        vec3 deep = vec3(0.15, 0.1, 0.35);     // deep violet
        vec3 mid  = vec3(0.35, 0.3, 0.8);      // blue-violet
        vec3 hot  = vec3(0.7, 0.8, 1.0);       // hot blue-white
        vec3 glow = vec3(0.6, 0.4, 1.0);       // magnetic glow violet

        col = mix(deep, mid, clamp(structure, 0.0, 1.0));
        col = mix(col, hot, smoothstep(0.6, 1.1, structure));
        col += glow * cracks * 0.6;                       // glowing cracks
        col += vec3(0.5, 0.7, 1.0) * fieldLines * 0.15;    // field lines
        col += vec3(0.8, 0.9, 1.0) * polarCap * 1.5;      // hot polar caps

        // Limb darkening
        float limbDark = 0.5 + 0.5 * cosTheta;
        col *= limbDark;

        // High brightness (neutron star is extremely hot/energetic)
        col *= 1.8;
        col = pow(col, vec3(0.85));  // slight gamma for contrast

        alpha = 1.0;
    } else {
        // Tight intense glow
        float distFromEdge = nr - 1.0;
        float falloff = exp(-distFromEdge * 5.0);
        col = vec3(0.5, 0.45, 1.0) * falloff * 1.2;
        alpha = falloff * 0.7;
    }
    return vec4(col, alpha);
}

// Neutron star polar jets: along magnetic axis (tilted from rotation axis)
vec3 nsJets(vec2 d, float nr, float angle, float t, float R) {
    // Magnetic axis direction (screen-space), combines tilt + rotation
    float axisAngle = uNSMagAxisTilt + uNSRotation * 0.3;
    vec2 ax = vec2(cos(axisAngle), sin(axisAngle));
    vec2 perpAx = vec2(-ax.y, ax.x);

    float along = dot(d, ax) / R;     // signed distance along axis (in R units)
    float perp = abs(dot(d, perpAx)) / R;

    // Two-sided jets (positive and negative along axis)
    float absAlong = abs(along);

    // Jet length and width
    float jetLen = uNSJetLen * 8.0;    // in R units
    float jetWidth = 0.35;

    if (absAlong < 0.3 || absAlong > jetLen) return vec3(0.0);

    // Cone falloff: narrow at base, widens slightly, fades with length
    float widthAt = jetWidth * (0.3 + 0.7 * (absAlong / jetLen));
    float cone = exp(-perp * perp / (2.0 * widthAt * widthAt));

    // Length envelope: bright near base, fades outward
    float lenEnv = smoothstep(0.3, 0.6, absAlong) * smoothstep(jetLen, jetLen * 0.5, absAlong);

    // Flow knots moving outward along jet
    float flow = 0.5 + 0.5 * sin(along * 1.2 - t * 4.0);
    float flow2 = 0.5 + 0.5 * sin(along * 2.5 - t * 6.0 + 1.5);
    float streak = mix(flow, flow2, 0.4);

    // Lighthouse sweep: jet brightness modulated by rotation (pulsar beam)
    float sweep = pow(max(0.0, cos(uNSRotation * uNSPulseRate)), 6.0);
    sweep = 0.3 + 0.7 * sweep;  // never fully dark

    float intensity = cone * lenEnv * streak * sweep;

    // Color: X-ray/gamma (blue-violet) + radio (visible white)
    vec3 gammaCol = vec3(0.55, 0.4, 1.0);    // gamma violet
    vec3 xrayCol = vec3(0.4, 0.6, 1.0);       // X-ray blue
    vec3 radioCol = vec3(0.9, 0.95, 1.0);     // radio white
    vec3 col = mix(gammaCol, xrayCol, streak * streak);
    col = mix(col, radioCol, cone * cone * 0.4);

    return col * intensity * uNSJetBright * 2.5;
}

// Pulsar beam: thin bright column along magnetic poles
vec3 nsPulsarBeam(vec2 d, float nr, float t, float R) {
    float axisAngle = uNSMagAxisTilt + uNSRotation * 0.3;
    vec2 ax = vec2(cos(axisAngle), sin(axisAngle));
    vec2 perpAx = vec2(-ax.y, ax.x);

    float along = dot(d, ax) / R;
    float perp = abs(dot(d, perpAx)) / R;
    float absAlong = abs(along);

    float beamLen = uNSJetLen * 12.0;
    if (absAlong < 0.2 || absAlong > beamLen) return vec3(0.0);

    float beamWidth = 0.12;
    float beam = exp(-perp * perp / (2.0 * beamWidth * beamWidth));
    float lenEnv = smoothstep(0.2, 0.5, absAlong) * smoothstep(beamLen, beamLen * 0.4, absAlong);

    // Sharp pulsation
    float pulse = pow(max(0.0, cos(uNSRotation * uNSPulseRate * 1.0)), 16.0);

    return vec3(0.9, 0.95, 1.0) * beam * lenEnv * pulse * uNSJetBright * 3.0;
}

// Supernova: expanding shock shell + central flash + radial spokes
vec3 supernovaShell(vec2 d, float nr, float angle, float t, float R) {
    float p = clamp(uSupernovaProgress, 0.0, 1.0);
    if (p < 0.001) return vec3(0.0);

    // Expanding shock radius (in R units)
    float shockR = p * 10.0;
    float shellW = 0.6 + p * 1.5;

    // Shock shell ring
    float shell = exp(-pow((nr - shockR) / shellW, 2.0));

    // Central white flash (decays as shell expands)
    float flash = (1.0 - p) * exp(-nr * 3.0) * 2.0;

    // Radial spokes (filamentary explosion structure)
    float spokes = pow(0.5 + 0.5 * cos(angle * 16.0 + shockR * 10.0), 4.0);
    spokes *= exp(-pow((nr - shockR * 0.8) / (shellW * 2.0), 2.0));

    // Bright core glow during early explosion
    float core = exp(-nr * 4.0) * (1.0 - p * 0.5);

    float intensity = (shell * 1.5 + flash + spokes * 0.8 + core * 0.6);

    // Color: white-hot core, orange-red shock, blue-violet spokes
    vec3 hot = vec3(1.0, 0.95, 0.85);
    vec3 fire = vec3(1.0, 0.5, 0.15);
    vec3 plasma = vec3(0.6, 0.4, 1.0);

    vec3 col = mix(fire, hot, flash * 0.5 + core * 0.5);
    col = mix(col, plasma, spokes * 0.4);

    return col * intensity * 3.5;
}


// Mini BH rendering: exact same pipeline as main BH (blackhole.glsl).
// Key: uses B_CRIT scaling so gravitational field matches visual size.
// d is already aspect-corrected (d.x *= aspect in main()).
vec4 miniBHColor(vec2 d, float nr, float angle, float t, float R, float aspect) {
    const float B_CRIT    = 2.5980762;   // 3*sqrt(3)/2 — critical impact parameter
    const float LENS_DEPTH = 13.0;

    float rin  = 1.5;
    float rout = 4.0;
    float bmax = rout + 3.0;
    float Z0   = max(14.0, rout + 5.0);  // same as main BH

    // B_CRIT scaling: at shadow edge (|d|=R), impact parameter = B_CRIT
    float W  = B_CRIT / max(R, 1e-5);
    vec2  pr = d * W;
    float b  = length(pr);

    // Event horizon
    if (nr < 1.0) {
        return vec4(0.0, 0.0, 0.0, uBHFormationProgress);
    }

    float incl = uMiniBHDiskIncl;
    float ci = cos(incl), si = sin(incl);
    vec3 diskN  = vec3(0.0, si, ci);
    vec3 diskE2 = vec3(0.0, ci, -si);

    vec3  emitc = vec3(0.0);
    float trans = 1.0;
    vec3  bg    = vec3(0.0);

    // ========== FAR FIELD: analytical deflection (from main BH) ==========
    if (b >= bmax) {
        float u    = Z0 * inversesqrt(Z0 * Z0 + b * b);
        float defl = (2.0 / (W * W)) / max(length(d), 1e-4)
                   * (1.29 * u + 0.07) * max(LENS_DEPTH - 2.14 * u + 0.75, 0.0)
                   * uMiniBHLensStrength;
        vec2 dir2  = d / max(length(d), 1e-5);
        float ab   = 0.035 * smoothstep(1.0, 2.0, b / bmax);
        for (int i = 0; i < 3; i++) {
            float k = 1.0 + (float(i) - 1.0) * ab;
            vec2 sp = d - dir2 * defl * k;
            vec2 sampleUV = uStarPos + vec2(sp.x / aspect, sp.y);
            sampleUV = clamp(sampleUV, vec2(0.001), vec2(0.999));
            bg[i] = texture(uFBTexture, sampleUV)[i];
        }
    }
    // ========== NEAR FIELD: full geodesic trace (from main BH) ==========
    else {
        vec3 x = vec3(pr, Z0);
        vec3 v = vec3(0.0, 0.0, -1.0);
        float h2 = dot(pr, pr);

        bool  captured = false;
        float sPrev = dot(x, diskN);
        vec3  xPrev = x;

        for (int i = 0; i < 64; i++) {
            float r2 = dot(x, x);
            if (r2 < 1.0) { captured = true; break; }
            if (x.z < -Z0 && v.z < 0.0) break;
            if (r2 > 4.0 * Z0 * Z0) break;

            float r  = sqrt(r2);
            float dt = clamp(0.16 * r, 0.03, 1.5);
            vec3 a = -1.5 * h2 * x / (r2 * r2 * r);

            v += a * (0.5 * dt);
            x += v * dt;
            r2 = dot(x, x);
            r  = sqrt(r2);
            a  = -1.5 * h2 * x / (r2 * r2 * r);
            v += a * (0.5 * dt);

            float s = dot(x, diskN);
            if (s * sPrev < 0.0 && trans > 0.02) {
                float tc = sPrev / (sPrev - s);
                vec3 xc = mix(xPrev, x, tc);
                float rc = length(xc);

                if (rc > rin && rc < rout) {
                    float band = smoothstep(rin, rin * 1.25, rc)
                               * (1.0 - smoothstep(rout * 0.70, rout, rc));

                    float phi   = atan(dot(xc, diskE2), xc.x);
                    float turns = phi / 6.2831853;
                    float kep   = pow(rin / rc, 1.5);
                    float gloc  = sqrt(max(1.0 - 1.5 / rc, 0.02));
                    float swirl = rc * 7.0 * 0.12 - t * kep * 5.0 * gloc * 0.08;
                    float streaks = valueNoise(vec2(rc * 2.8, turns * 19.0 + swirl * 3.0)) * 0.65 +
                                    valueNoise(vec2(rc * 1.0, turns * 9.0  + swirl * 1.5 + 7.0)) * 0.35;
                    streaks = 0.35 + 1.6 * streaks * streaks;

                    vec3 gasdir = normalize(cross(diskN, xc));
                    float beta  = clamp(inversesqrt(max(2.0 * (rc - 1.0), 0.2)), 0.0, 0.99);
                    float g     = gloc / max(1.0 + beta * dot(gasdir, normalize(v)), 0.05);
                    g = mix(1.0, g, 0.6);

                    float xpr   = max(1.0 - sqrt(rin / rc), 0.0);
                    float tprof = pow(rin / rc, 0.75) * pow(xpr, 0.25) / 0.488;

                    float tEff = 8000.0 * tprof * g;
                    vec3 cbb;
                    if (tEff > 20000.0) cbb = vec3(1.0, 0.95, 0.9);
                    else if (tEff > 10000.0) cbb = vec3(1.0, 0.85, 0.7);
                    else if (tEff > 6000.0) cbb = vec3(1.0, 0.75, 0.5);
                    else if (tEff > 4000.0) cbb = vec3(1.0, 0.6, 0.3);
                    else cbb = vec3(0.8, 0.4, 0.2);
                    cbb *= tEff / 8000.0;

                    float boost = pow(max(g, 0.01), 2.5);
                    float density = band * streaks;
                    emitc += trans * cbb * (2.2 * 2.5 * density * tprof * tprof * boost);
                    trans *= 1.0 - clamp(0.85 * density, 0.0, 1.0);
                }
            }
            sPrev = s;
            xPrev = x;
        }
        if (!captured && dot(x, x) < 4.0) captured = true;

        if (!captured) {
            vec3 dir = normalize(v);
            if (dir.z < -0.05) {
                float tpl = (-LENS_DEPTH - x.z) / dir.z;
                vec3 hp = x + dir * tpl;
                vec2 screenOffset = hp.xy / W;
                vec2 sampleUV = uStarPos + vec2(screenOffset.x / aspect, screenOffset.y);
                sampleUV = clamp(sampleUV, vec2(0.001), vec2(0.999));
                bg = texture(uFBTexture, sampleUV).rgb;
            }
        }
    }

    // ---- Einstein ring near critical impact parameter ----
    float ringDist = abs(b - B_CRIT) / B_CRIT;
    float ring = exp(-pow(ringDist / 0.04, 2.0));
    emitc += vec3(3.0, 2.5, 2.0) * ring * uMiniBHRingBright;

    vec3 col = bg * trans + (vec3(1.0) - exp(-emitc * 1.4));

    float alpha = 1.0;
    if (b > bmax) alpha *= exp(-(b - bmax) * 0.5);
    alpha *= uBHFormationProgress;

    return vec4(col * uBHFormationProgress, alpha);
}

// Gravitational wave ripples: expanding rings that DISTORT space as they pass
// Each ripple lens-distorts the background by strongly displacing UV coordinates
vec3 gwRipples(vec2 d, float nr, float t, float R, float aspect) {
    vec2 mid = (uStarPos + uBHPos) * 0.5;
    vec2 toMid = vUV - mid;
    toMid.x *= aspect;
    float distMid = length(toMid);

    vec3 result = vec3(0.0);
    
    for (int i = 0; i < 3; i++) {
        float phase = t * 0.8 + float(i) * 1.2;
        float ringDist = mod(phase, 3.0) * 0.15; // expanding radius
        float ringWidth = 0.04;
        float ring = exp(-pow((distMid - ringDist) / ringWidth, 2.0));
        float fade = 1.0 - ringDist / 0.45;
        if (fade <= 0.0) continue;
        
        // Strong radial displacement: push pixels outward/inward as ripple passes
        // Wave phase: 0 at ring center, +/- at edges
        float wavePhase = (distMid - ringDist) / ringWidth;
        float displacement = sin(wavePhase * 3.14159) * uGWRippleStrength * 0.15;
        
        // Apply displacement in radial direction
        vec2 dir = toMid / max(distMid, 0.001);
        vec2 distortedUV = vUV + vec2(dir.x * displacement / aspect, dir.y * displacement);
        distortedUV = clamp(distortedUV, vec2(0.001), vec2(0.999));
        
        // Sample background with distortion
        vec3 distortedBg = texture(uFBTexture, distortedUV).rgb;
        
        // The ripple effect: visible distortion + faint blue glow
        float visibility = ring * fade;
        result += distortedBg * visibility * 0.8;
        result += vec3(0.2, 0.4, 0.8) * visibility * uGWRippleStrength * 0.2;
    }
    
    return result;
}

// Accretion bridge: turbulent matter stream between two BHs
vec3 accretionBridge(vec2 d, float nr, float t, float R, float aspect) {
    // Vector from mini BH to main BH
    vec2 ab = uBHPos - uStarPos;
    float abLen = length(ab);
    if (abLen < 0.001) return vec3(0.0);

    // Project current pixel onto the bridge line
    vec2 toPixel = vUV - uStarPos;
    toPixel.x *= aspect;
    vec2 abAspect = vec2(ab.x * aspect, ab.y);
    float along = dot(toPixel, abAspect / abLen); // 0 at mini BH, 1 at main BH
    vec2 perp = toPixel - along * abAspect;
    float perpDist = length(perp);

    // Only render between the two BHs
    if (along < 0.1 || along > 0.9) return vec3(0.0);

    // Turbulent flow: varying width along the bridge
    float widthBase = R * 0.8;
    float turbulence = sin(along * 8.0 + t * 3.0) * 0.3 + sin(along * 15.0 - t * 2.0) * 0.15;
    float bridgeWidth = widthBase * (1.0 + turbulence);
    float bridge = exp(-pow(perpDist / bridgeWidth, 2.0));

    // Hot plasma color with flow animation
    float flow = sin(along * 6.0 - t * 4.0 + perpDist * 20.0);
    vec3 hotCol = vec3(1.0, 0.8, 0.5);
    vec3 coolCol = vec3(0.5, 0.6, 1.0);
    vec3 bridgeCol = mix(hotCol, coolCol, along);

    return bridgeCol * bridge * uAccretionBridgeBright * (0.7 + 0.3 * flow);
}

void main() {
    float aspect = uResolution.x / uResolution.y;
    vec2 d = vUV - uStarPos;
    d.x *= aspect;

    float R = max(uStarSize, 1e-5);

    // ===== 潮汐几何：形变发生在密度域（D2），不再压扁采样坐标 =====
    // Why: 旧实现把 d 做逆向仿射，产出的是"几何硬边椭圆"——正是用户厌恶的形态。
    //      体积气体应在密度域各向异性：沿潮汐轴 σ_A 拉长、垂直方向 σ_P 压缩。
    //      唯一驱动源 uTidalAmp（CPU 由剥离进度 p 积分得到，有界 [0, tidalStretch]）。
    vec2 bd = uBHPos - uStarPos;
    bd.x *= aspect;
    float bdLen = length(bd);
    vec2 bdNorm = (bdLen > 1e-4) ? bd / bdLen : vec2(1.0, 0.0);
    vec2 bdPerp = vec2(-bdNorm.y, bdNorm.x);

    float amp    = max(uTidalAmp, 0.0);
    float strip  = clamp(uTidalStrip, 0.0, 1.0);
    float pTidal = clamp(uTidalProgress, 0.0, 1.0);

    // 潮汐归一化坐标：ξ 沿"指向黑洞"轴（正 = 朝黑洞），η 垂直
    float xi  = dot(d, bdNorm) / R;
    float eta = dot(d, bdPerp) / R;

    // 【D5 交界不变量】与 energy_stream_frag.glsl 逐字同源（两侧改一处忘一处 → 流起点立刻错位）：
    //   L_nose(amp) = R·(1 + amp)               本体沿潮汐轴的可见半长
    //   W_nose(amp) = R·max(1 - 0.30·amp, 0.35)  本体在鼻尖处的可见半宽
    // K = sqrt(ln(1/ε))，ε = 0.05：密度降到 5% 处即"可见边界"。
    // Why: 历史值 1.7315 是笔误（1.7315²=2.9981 ≠ ln20=2.99573）。此处用精确值
    //      sqrt(ln(1/0.05)) = 1.730818，与 tidal_model_test.cpp 的 kK 同一数值（RK3）。
    //      刻意写成字面常量而非 sqrt(log(...))：GLSL 是运行时编译，构建期无法校验，
    //      常量函数调用若被驱动拒绝只能在启动日志暴露（V6），故取零风险写法。
    const float K = 1.730818;
    float sigA = (1.0 + amp) / K;                    // σ_A（归一化）：= L_nose/K/R
    float sigP = max(1.0 - 0.30 * amp, 0.35) / K;    // σ_P（归一化）：= W_nose/K/R
    float q2   = (xi * xi) / (sigA * sigA) + (eta * eta) / (sigP * sigP);

    // amp = 0 时 sigA = sigP = 1/K → q2 = (nr·K)²，可见半径恰为 1.000·R（基线尺寸不变）
    float nr = length(d) / R;
    float angle = atan(d.y, d.x);
    float t = uTime;

    // ===== EMISSIVE PASS (additive blending) — jets / supernova / tension =====
    if (uPassMode == 1) {
        vec3 e = vec3(0.0);

        // Supernova explosion shell
        if (uSupernovaProgress >= 0.0) {
            e += supernovaShell(d, nr, angle, t, R);
        }

        // Neutron star polar jets + pulsar beam
        if (uStarStage == 2) {
            e += nsJets(d, nr, angle, t, R);
            e += nsPulsarBeam(d, nr, t, R);
        }

        // Collapse heating glow + tension sparks (any collapsing stage)
        if (uCollapseProgress > 0.0) {
            float glow = exp(-nr * 2.0) * uCollapseProgress;
            e += vec3(1.0, 0.7, 0.4) * glow * 0.9;
            // Fine tension sparks radiating outward
            float sparks = pow(0.5 + 0.5 * cos(angle * 24.0), 4.0);
            float sparkRing = exp(-pow((nr - 1.0 - uCollapseProgress * 2.5) / 0.35, 2.0));
            e += vec3(0.7, 0.8, 1.0) * sparks * sparkRing * uCollapseTension * 2.5;
        }

        // Dual black hole interactions (gravitational waves + accretion bridge)
        if (uStarStage == 3 && uDualBHActive > 0.5) {
            e += gwRipples(d, nr, t, R, aspect);
            e += accretionBridge(d, nr, t, R, aspect);
        }

        if (e.r + e.g + e.b < 0.001) discard;
        fragColor = vec4(e * uStarAlpha, 1.0);
        return;
    }

    // ===== EVOLUTION STAGE BODY PASSES (alpha blend) =====
    if (uStarStage == 3) {
        vec4 bh = miniBHColor(d, nr, angle, t, R, aspect);
        fragColor = vec4(bh.rgb, bh.a * uStarAlpha);
        return;
    }
    if (uStarStage == 1) {
        vec4 wd = whiteDwarfColor(d, nr, angle, t, R);
        fragColor = vec4(wd.rgb, wd.a * uStarAlpha);
        return;
    }
    if (uStarStage == 2) {
        vec4 ns = neutronStarColor(d, nr, angle, t, R);
        fragColor = vec4(ns.rgb, ns.a * uStarAlpha);
        return;
    }

    // ===== NORMAL STAR (stage 0)：分层体积气体（L1 光球 / L2 色球 / L3 日冕）=====
    // Why: 旧版把 photoAlpha 硬编码为 1.0，再经 finalAlpha = max(...) 输出 → 硬边不透明盘，
    //      这就是"实心光球"。现在改为密度场 + Beer–Lambert 吸收：边缘自然变薄透光、
    //      内部有可透视的层次，噪声调制的是"密度"而不再是"颜色"（R1 / G1 / G2）。
    float pulse  = 1.0 + 0.03 * sin(t * 0.5) + 0.015 * sin(t * 1.2);
    float q2p    = q2 / (pulse * pulse);          // 呼吸：整体半径 ±4%，形状不变量保持
    float nrShape = sqrt(q2p) / K;                // 各向异性归一化半径（amp=0 时恒等于 nr）

    // 【F2】裁剪域收紧：日冕门 2.6R + 日珥弧顶 3.2R → 1.8×uCoronaSize(默认 3.6R) 足够。
    // 旧值 3.0× 使默认档裁到 6R（=150% 屏高），是"全屏遮罩"的覆盖域来源之一。
    if (nrShape > uCoronaSize * 1.8) discard;

    // 本体消散（T5）：同一份 p 已在流侧注入宽度/亮度，质量不凭空消失
    float fadeT = smoothstep(0.35, 1.0, pTidal) * clamp(uTidalBodyFade, 0.0, 1.0);

    // ============================================================
    // L1 光球 + L2 色球：密度场（重噪声仅在可见区内计算 → 性能闸门 G10）
    // ============================================================
    vec3  emitBody = vec3(0.0);
    float absorb   = 0.0;

    if (q2p <= 6.0) {
        // 结构场：对流胞 / 网络 / 米粒 / 黑子（时间偏移使其静止时也持续演化 → G2）
        vec2 sp = vec2(xi, eta);
        float fil1 = fbm(sp * 3.0  + vec2( t * 0.020, -t * 0.012), 4);
        float fil2 = fbm(sp * 7.0  + vec2(-t * 0.035,  t * 0.028) + 100.0, 3);
        float fil3 = fbm(sp * 15.0 + vec2( t * 0.050,  t * 0.040) + 200.0, 2);
        float filaments = fil1 * 0.5 + fil2 * 0.3 + fil3 * 0.2;

        float network = smoothstep(0.30, 0.80, fbm(sp * 4.0 + vec2(t * 0.015) + 300.0, 4));
        float cells   = smoothstep(0.00, 0.50, fbm(sp * 2.5 + vec2(t * 0.010) + 400.0, 3)) * 0.5;
        float sunspot = smoothstep(0.58, 0.68, fbm(sp * 1.8 + vec2(t * 0.005) + 500.0, 4)) * 0.6;

        float structure = filaments * 0.5 + network * 0.35 + cells * 0.15;
        structure = mix(0.25, 1.5, structure) * (1.0 - sunspot);

        // 拉丝场（R4 / G8）：横向高频、沿潮汐轴低频 → 条纹沿"指向黑洞"方向排列；
        // 相速度随 strip 增大 → 撕裂越深，"被抽走"的动势越明显。
        vec2  qq = vec2(eta, xi);
        float streakPhase = t * (0.35 + uStreakSpeed * strip);
        float streak = fbm(vec2(qq.y * 1.2, qq.x * 3.0 - streakPhase), 3);

        // F(noise)：strip=0 退化为纯 structure（常态）；uStreakAmount=0.9 时等价于 0.55+0.90·streak
        float F = structure * (1.0 + uStreakAmount * (streak - 0.5) * strip);

        // ---- L1 光球：较厚（k₁ = uGasOpacity）----
        float rho1 = exp(-q2p) * F;
        float a1   = 1.0 - exp(-uGasOpacity * rho1);
        a1 *= (1.0 - fadeT);

        // ---- L2 色球：同一密度场，尺度 ×uChromoScale，吸收更薄（k₂ = 0.9）----
        float rho2 = exp(-q2p / (uChromoScale * uChromoScale)) * F;
        float a2   = 1.0 - exp(-0.9 * rho2);

        // 两层叠加吸收（能量守恒：1-(1-α₁)(1-α₂)）
        absorb = 1.0 - (1.0 - a1) * (1.0 - a2);

        // 【F1b/G1 实机反馈修正】吸收（=本体不透明度）必须随本体一起结束：1.0R 起平滑归零。
        // Why: exp(-q²) 的密度尾部经复合 max() 后，会在本体边界（nrShape=1）外留出
        //      0.1~0.3 的半透明"遮罩盘"，且在 q2p≤6 早退边界（≈1.415R）形成 alpha 断层
        //      —— 用户截图中恒星外围那圈大透明盘的主体。发射（日冕/日珥）不受此门限制。
        // G1 第二轮实机反馈：旧窗口 (1.05,1.35) 起点偏晚，1.05R 处 absorb 仍 0.33，
        //   与发射地板叠加后白色遮罩仍可见 → 窗口提前到 (1.0,1.2)，边界即开始收。
        absorb *= 1.0 - smoothstep(1.0, 1.2, nrShape);

        // ---- 发射 ----
        // chord = 视向弦长 → 边缘变薄天然产生临边昏暗，这是"球"的立体感来源
        float chord   = sqrt(max(1.0 - min(q2p, 1.0), 0.0));
        vec3  coreCol = mix(uStarColor, vec3(1.0), 0.85);   // 与 energy_stream_frag.glsl 的 bodyHotCol 同源
        emitBody  = mix(uStarColor, coreCol, smoothstep(0.45, 1.0, rho1))
                  * (0.75 + 0.85 * rho1) * (0.45 + 0.55 * chord);
        emitBody += uStarColor * rho2 * uCoronaBright * 1.10;
        // 【G1 实机反馈修正】发射域门：emitBody 必须与本体一起结束。
        // Why: chord 项在体外（q2p>1）只衰减到 0.45、(0.75+0.85·rho1) → 0.75，
        //   乘积给出 0.34×uStarColor 的非零地板，一直铺到 q2p≤6 早退边界（1.415R）；
        //   再加 L2 色球（1.75× 尺度）的尾部 rho2≈0.32，合成 1.0~1.4R 环带上约 28% 的
        //   白色叠加 —— 这才是"最外层白色遮罩"的主体（F1b 只门了吸收，发射漏了）。
        // 门从 1.0R 起算、1.25R 归零：与吸收门同域，日冕/日珥发射不受影响。
        emitBody *= 1.0 - smoothstep(1.0, 1.25, nrShape);
        emitBody *= (1.0 - 0.85 * fadeT);
    }

    // ============================================================
    // L3 日冕：纯发射，任何距离都算（廉价）
    // 【F1 实机反馈修正】v3.0 把衰减写成 exp(-x/1.10)，衰减率 0.91 仅为旧硬编码 3.0 的 1/3.3，
    //   辉光在 2R 外仍有 0.16 强度，再经 finalAlpha = max(..., glowMask*0.5) 复合，
    //   形成"罩住半个屏幕的半透明遮罩"（用户截图确认）。改回乘法衰减率：
    //     glow = exp(-x · uCoronaFalloff · 2.8)
    //   已保存的旧配置值 1.10 → 衰减率 3.08 ≈ 旧外观，无需配置迁移；滑块越大 = 越薄。
    // 【F2】日冕物理上是"贴边薄层"：1.4R 外平滑归零，不再无限外扩。
    // ============================================================
    vec3  glowCol = vec3(0.0);
    float glowMask = 0.0;
    {
        float distFromEdge = max(nrShape - 1.0, 0.0);
        float falloff = exp(-distFromEdge * max(uCoronaFalloff, 0.05) * 2.8);

        float streamers = fbm(vec2(angle * 8.0 + t * 0.03, distFromEdge * 6.0), 4);
        streamers = 0.18 + 0.82 * streamers;

        // 【G1 第二轮实机反馈】gate 从 (1.4,2.6) 收紧到 (1.1,2.0)。
        // Why: F1b 只门吸收、G1 补门发射后，1.1~2.4R 仍有 glowMask×0.5 的 alpha 残余
        //   （1.4R 处 ≈0.09），与旧 gate 的宽容起点叠加，遮罩边缘仍隐约可见。
        // 日冕物理上就是贴边薄层：1.1R 起收、2.0R 归零（falloff 在 2R 处已 <1e-3，无跳变）。
        float coronaGate = 1.0 - smoothstep(1.1, 2.0, distFromEdge);
        glowMask = falloff * streamers * coronaGate;
        glowCol = uStarColor * glowMask * uCoronaBright;
        glowCol = mix(glowCol, vec3(1.0, 0.85, 0.55), 0.25 * glowMask);
    }

    // ============================================================
    // PROMINENCES - erupting plasma arcs
    // ============================================================
    vec3 promCol = vec3(0.0);
    float prominence = 0.0;

    for (int i = 0; i < 4; i++) {
        float seed = float(i) * 47.3;
        float cycleDuration = 6.0 + hash21(vec2(seed, 0.0)) * 6.0; // 6-12 seconds
        float phase = floor(t / cycleDuration + seed);
        float life = fract(t / cycleDuration + seed);
        
        // Eruption envelope: birth, rise, peak, fall, fade
        float env = smoothstep(0.0, 0.15, life) * smoothstep(1.0, 0.55, life);
        float rise = smoothstep(0.0, 0.35, life); // rapid rise
        
        // Base angle where prominence erupts from surface
        float baseAngle = hash21(vec2(seed, phase)) * 6.28318;
        
        // Maximum eruption height grows then shrinks
        float maxHeight = (1.0 + hash21(vec2(seed + 5.0, phase)) * 1.0) * env * rise;
        
        // How much the arc leans left/right
        float lean = (hash21(vec2(seed + 7.0, phase)) - 0.5) * 0.8;
        
        // Thickness at base and top
        float baseThick = 0.10 + hash21(vec2(seed + 11.0, phase)) * 0.08;
        
        // Sample the prominence curve to find nearest point
        float bestDist = 1e10;
        float bestS = 0.0;
        float bestThick = 0.0;
        
        const int SAMPLES = 32;
        for (int s = 0; s < SAMPLES; s++) {
            float sf = float(s) / float(SAMPLES - 1);
            
            // Arc shape: rises from surface, peaks, then falls back
            float h = sin(sf * 3.14159);
            float r = 1.0 + h * maxHeight;
            
            // Add irregular wobble along the arc
            float wobble = sin(sf * 12.56 + seed * 10.0) * 0.05 * h;
            wobble += fbm(vec2(sf * 8.0, t * 0.5 + seed), 3) * 0.08 * h;
            
            // Lean: angle shifts with height
            float a = baseAngle + lean * h + wobble;
            
            // Angular distance on wrapped circle
            float da = abs(mod(angle - a + 3.14159, 6.28318) - 3.14159);
            // Approximate arc-length distance at radius r（半径用各向异性 nrShape，随撕裂形变）
            float d = sqrt(da * da * r * r + (nrShape - r) * (nrShape - r));
            
            // Thickness varies along arc: wider at base, narrower at top
            float thick = baseThick * (1.0 - 0.5 * h) * env;
            
            if (d / thick < bestDist) {
                bestDist = d / thick;
                bestS = sf;
                bestThick = thick;
            }
        }
        
        if (bestDist < 1.2 && env > 0.01) {
            // Plasma filament structure inside the prominence
            float plasma = turbulent(vec2(bestS * 15.0, t * 0.8 + seed)) * 0.5 + 0.5;
            plasma *= 0.5 + 0.5 * sin(bestS * 31.4 + t * 2.0 + seed);
            
            // Intense flickering combustion
            float flicker = 0.6 + 0.4 * sin(t * 8.0 + bestS * 20.0 + seed * 30.0);
            flicker *= 0.7 + 0.3 * sin(t * 23.0 + seed * 17.0);
            
            // Mask: soft at edges
            float mask = smoothstep(1.2, 0.0, bestDist);
            
            float intensity = mask * plasma * flicker * env;
            prominence = max(prominence, intensity);
            
            // Color: hot yellow-white core, orange-red body
            float core = smoothstep(0.5, 0.0, bestDist);
            vec3 pColor = mix(vec3(1.0, 0.25, 0.03), vec3(1.0, 0.7, 0.25), core);
            pColor = mix(pColor, vec3(1.0, 0.95, 0.85), core * core * 0.6);
            
            promCol = max(promCol, pColor * intensity * 5.0);
        }
    }

    // ============================================================
    // SOLAR FLARES - bright localized eruptions
    // ============================================================
    vec3 flareCol = vec3(0.0);
    float flare = 0.0;

    for (int i = 0; i < 5; i++) {
        float seed = float(i) * 53.3;
        float fp = floor(t * 0.25 + seed);
        float flife = fract(t * 0.25 + seed * 0.5);
        float fenv = smoothstep(0.0, 0.1, flife) * smoothstep(1.0, 0.2, flife);

        float fa = hash21(vec2(seed, fp)) * 6.28318;
        float fr = 0.2 + hash21(vec2(seed + 11.0, fp)) * 0.7;
        float fw = 0.05 + hash21(vec2(seed + 19.0, fp)) * 0.06;

        float ad = abs(mod(angle - fa + 3.14159, 6.28318) - 3.14159);
        float rd = abs(nrShape - fr);

        if (ad < fw && rd < fw * 0.5 && fenv > 0.01) {
            float fk = (1.0 - ad / fw) * (1.0 - rd / (fw * 0.5)) * fenv;
            fk *= 0.7 + 0.3 * sin(t * 10.0 + seed * 30.0);
            flare = max(flare, fk);
        }
    }

    flareCol = vec3(1.0, 0.95, 0.85) * flare * 4.0;

    // ============================================================
    // COMPOSITE
    // ============================================================
    // 呼吸（pulse）已提前作用在 q2p 上，这里不再改 nr —— 避免扰动 D5 形状不变量。
    vec3 finalColor = emitBody + glowCol + promCol + flareCol;

    // Evolution heating: as the star collapses it heats toward white-hot;
    // as it expands it brightens. (stage 0 only; size is driven by CPU)
    if (uCollapseProgress > 0.0) {
        finalColor = mix(finalColor, vec3(1.0, 0.95, 0.9), uCollapseProgress * 0.55);
        finalColor *= 1.0 + uCollapseProgress * 0.8;
    }
    if (uExpandProgress > 0.0) {
        finalColor *= 1.0 + uExpandProgress * 0.5;
    }

    // 覆盖度：吸收（吸收层）∪ 日冕/日珥/耀斑的发射掩膜。不再有硬编码的 1.0 → 软边（G1）
    float finalAlpha = max(max(absorb, glowMask * 0.5), max(prominence, flare * 0.9));
    finalAlpha *= uStarAlpha;

    // Tonemap
    finalColor = finalColor / (1.0 + finalColor * 0.12);

    if (finalAlpha < 0.005) discard;
    fragColor = vec4(finalColor, finalAlpha);
}
