#version 460 core

// Desktop mode fragment header — includes desktop texture and all uniforms

uniform vec3  iResolution;
uniform float iTime;
uniform vec4  iDate;
uniform vec4  iMouse;
uniform vec4  iCurrentCursorColor;
uniform vec4  iPreviousCursorColor;
uniform float iTimeCursorChange;

uniform sampler2D iChannel0;
uniform sampler2D iChannel1;   // Particle FBO texture (offscreen rendered)

// Desktop icon devour: whole-desktop radial stretch + tangential swirl field
uniform int   uDevourEnable;    // 0 = 完全关闭（保持原行为）
uniform vec2  uDevourHoleUV;    // 黑洞中心 UV（Y-up，与 bhCx/bhCy 同空间）
uniform float uDevourStretch;   // 径向拉伸增益
uniform float uDevourSwirl;     // 切向漩涡增益

// Adjustable shader parameters (negative = use shader default)
uniform float uHoleRadius;
uniform float uDiskGain;
uniform float uDiskTemp;
uniform float uExposure;
uniform float uSpeed;
uniform float uStarGain;
uniform float uDiskIncl;

// Advanced disk parameters
uniform float uDiskRoll;
uniform float uDiskInner;
uniform float uDiskOuter;
uniform float uDiskOpac;
uniform float uDopplerMix;
uniform float uDiskBeam;
uniform float uDiskContrast;
uniform float uDiskWind;
uniform float uDiskSpeed;

// Lens & gravity parameters
uniform float uLensDepth;
uniform float uDilationMin;

// Disk color gradient (0 = off, 1 = strong)
uniform float uColorGradient;

// Time dilation
uniform float uTimeDilation;

// Relativistic jets
uniform float uJetEnabled;
uniform float uJetLength;
uniform float uJetWidth;
uniform float uJetSpeed;
uniform float uJetBrightness;

// Playback control
uniform int   uPlayMode;
uniform float uSlotSec;
uniform int   uPresetCount;
uniform float uPresetOffset;

// Born/death progress
uniform float uBornProgress;

// Fixed size mode
uniform int   uFixedSize;
uniform float uFixedLevel;

// Demo mode toggle (1.0 = enabled, 0.0 = disabled)
uniform float uDemoMode;

// Home position
uniform float uHomeX;
uniform float uHomeY;
uniform float uRandPhase;

// Star gravitational lensing (up to 3 stars)
uniform vec3  uStarPositions;  // xy = star0 UV, z = active count
uniform vec3  uStarSizes;      // x = star0 size, y = star1 size, z = star2 size
uniform float uStarLensStr;    // lensing strength

// Stellar evolution collapse effects (screen-wide)
uniform vec2  uShakeOffset;       // pixel offset for screen shake
uniform float uCollapseVignette;  // 0..1 screen-edge darkening during collapse
uniform float uCollapseTension;   // 0..1 fine tension lines during collapse

// Kerr metric parameters
uniform float uSpinParam;         // spin parameter a (0.0 ~ 0.99)
uniform float uKerrMode;          // 1.0 = Kerr metric, 0.0 = Schwarzschild
uniform int   uNumSteps;          // geodesic integration steps (48 ~ 256)

// Black hole merger
uniform float uMergerPhase;       // 0=idle, 1=inspiral, 2=merger, 3=ringdown
uniform float uMergerProgress;    // current phase progress 0..1
uniform float uMergerFlash;       // flash intensity 0..1
uniform float uMergerRingdownAmp; // ringdown oscillation amplitude
uniform float uMergerGWFreq;      // gravitational wave frequency
uniform vec2  uMergerBHOffset;    // main BH offset toward mini BH during merger
uniform vec2  uMergerCenter;      // merger center position (locked during merger)

// Preset arrays (14 x 64)
uniform float uPresetTemp[64];
uniform float uPresetIncl[64];
uniform float uPresetRoll[64];
uniform float uPresetInner[64];
uniform float uPresetOuter[64];
uniform float uPresetOpac[64];
uniform float uPresetDopp[64];
uniform float uPresetBeam[64];
uniform float uPresetGain[64];
uniform float uPresetContrast[64];
uniform float uPresetWind[64];
uniform float uPresetSpeed[64];
uniform float uPresetExpo[64];
uniform float uPresetStar[64];

// Output
out vec4 fragColor;

// Helper to sample desktop texture with Y-flip (DX11 to OpenGL conversion)
vec4 getDesktopTexture(vec2 uv) {
    return texture(iChannel0, vec2(uv.x, 1.0 - uv.y));
}

// Variant for flipped UV that's already computed
vec4 getDesktopTextureRaw(vec2 uv) {
    return texture(iChannel0, vec2(uv.x, 1.0 - uv.y));
}

// Sample particle texture at given UV (no Y-flip needed: FBO uses OpenGL convention)
vec4 getParticleTexture(vec2 uv) {
    return texture(iChannel1, uv);
}

// Desktop icon devour field: 把输出屏幕 UV 映射到"更远处"的源 UV。
// 径向拉伸（1/r 发散）导致源内容被拉长 → 图标呈面条状拖尾；叠加切向旋转 → 螺旋卷入。
// Why：uDevourStretch/Swirl 以"增益"为单位（滑块 ~[0,3]），SCALE 把 UV 量纲的
// 1/r 项压到合理视觉范围（典型视界半径 ~0.02-0.08 UV）。具体标定见计划书 §5.2 调参项。
vec2 devourWarpUV(vec2 uvIn) {
    if (uDevourEnable == 0) return uvIn;   // 关闭时严格保持原行为
    const float EPS   = 1e-4;
    const float SCALE = 0.02;
    vec2  d  = uvIn - uDevourHoleUV;
    float r  = length(d);
    float th = atan(d.y, d.x);
    float rSrc  = r * (1.0 + (uDevourStretch * SCALE) / (r + EPS));
    float thSrc = th + (uDevourSwirl * SCALE) / (r + EPS);
    return uDevourHoleUV + rSrc * vec2(cos(thSrc), sin(thSrc));
}

// Composite desktop + particles at a given UV (additive blending)
vec3 sampleBackground(vec2 suv) {
    // 只对整个桌面主画面采样生效；黑洞本体几何仍使用原始 uv，避免被拉伸场扭曲。
    suv = devourWarpUV(suv);
    vec3 desktop = getDesktopTexture(suv).rgb;
    vec3 particles = getParticleTexture(suv).rgb;
    return desktop + particles;
}
