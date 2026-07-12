#pragma once

#include "texture_source.h"

// Black hole shader parameters (3744 bytes, matches HLSL cbuffer layout)
struct BlackHoleUniforms {
    // Global state
    float iResolution[3];     // vec3
    float iTime;              // float
    float iDate[4];           // vec4
    float iMouse[4];          // vec4
    float iCurrentCursorColor[4];  // vec4
    float iPreviousCursorColor[4]; // vec4
    float iTimeCursorChange;  // float

    // 14 adjustable parameters
    float holeRadius;
    float diskGain;
    float diskTemp;
    float exposure;
    float speed;
    float starGain;
    float diskIncl;

    // Playback control
    int   playMode;           // 0=sequential, 1=cyclic, 2=random
    float slotSec;            // seconds per preset slot
    int   presetCount;

    // Born/death progress
    float bornProgress;       // 0→1 appear, 1→0 disappear

    // Fixed size mode
    int   fixedSize;          // 0=grow with time, 1=fixed
    float fixedLevel;         // 0.1~1.0

    // Home position
    float homeX;
    float homeY;

    // Preset arrays (14 x 64)
    float presetTemp[64];
    float presetIncl[64];
    float presetRoll[64];
    float presetInner[64];
    float presetOuter[64];
    float presetOpac[64];
    float presetDopp[64];
    float presetBeam[64];
    float presetGain[64];
    float presetContrast[64];
    float presetWind[64];
    float presetSpeed[64];
    float presetExpo[64];
    float presetStar[64];
};

// Abstract renderer interface
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool Init(int width, int height) = 0;
    virtual void Shutdown() = 0;
    virtual void Render(const TextureFrame& frame, const BlackHoleUniforms& uniforms) = 0;
    virtual void Resize(int newW, int newH) = 0;
};
