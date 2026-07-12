#version 330

// Preview mode fragment header — for Qt FBO preview (no desktop texture)

uniform vec3  iResolution;
uniform float iTime;
uniform vec4  iDate;
uniform vec4  iMouse;
uniform vec4  iCurrentCursorColor;
uniform vec4  iPreviousCursorColor;
uniform float iTimeCursorChange;

// Adjustable shader parameters (negative = use shader default)
uniform float uHoleRadius;
uniform float uDiskGain;
uniform float uDiskTemp;
uniform float uExposure;
uniform float uSpeed;
uniform float uStarGain;
uniform float uDiskIncl;

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

// Home position
uniform float uHomeX;
uniform float uHomeY;
uniform float uRandPhase;

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
