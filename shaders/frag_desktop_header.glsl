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
