#include "win32_gl.h"
#include "gl_texture.h"
#include "capture_wgc.h"
#include "gui_config.h"
#include "monitors.h"
#include "star_texture.h"
#include "desktop_icons.h"
#include "window_devour.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <ctime>
#include <fstream>
#include <sstream>

// 在 main.cpp 的 #include 之后加入
#ifndef GL_BLEND
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_DEPTH_TEST 0x0B71
#endif

// ---- GL function declarations (dynamically loaded) ----
#define DECL_GL_FUNC(ret, name, ...) typedef ret (APIENTRY* PFN_##name)(__VA_ARGS__); static PFN_##name name = nullptr
#define LOAD_GL_FUNC(name) name = (PFN_##name)Win32GL_GetProcAddress(#name)

DECL_GL_FUNC(void, glClearColor, float, float, float, float);
DECL_GL_FUNC(void, glClear, unsigned int);
DECL_GL_FUNC(void, glEnable, unsigned int);
DECL_GL_FUNC(void, glBlendFunc, unsigned int, unsigned int);
DECL_GL_FUNC(unsigned int, glCreateProgram);
DECL_GL_FUNC(void, glLinkProgram, unsigned int);
DECL_GL_FUNC(void, glUseProgram, unsigned int);
DECL_GL_FUNC(int,  glGetUniformLocation, unsigned int, const char*);
DECL_GL_FUNC(void, glUniform1f, int, float);
DECL_GL_FUNC(void, glUniform1i, int, int);
DECL_GL_FUNC(void, glUniform1fv, int, int, const float*);
DECL_GL_FUNC(void, glUniform2f, int, float, float);
DECL_GL_FUNC(void, glUniform3f, int, float, float, float);
DECL_GL_FUNC(void, glUniform4f, int, float, float, float, float);
DECL_GL_FUNC(void, glVertexAttribPointer, unsigned int, int, unsigned int, unsigned char, int, const void*);
DECL_GL_FUNC(void, glEnableVertexAttribArray, unsigned int);
DECL_GL_FUNC(void, glDrawArrays, unsigned int, int, int);
DECL_GL_FUNC(unsigned int, glCreateShader, unsigned int);
DECL_GL_FUNC(void, glShaderSource, unsigned int, int, const char**, const int*);
DECL_GL_FUNC(void, glCompileShader, unsigned int);
DECL_GL_FUNC(void, glAttachShader, unsigned int, unsigned int);
DECL_GL_FUNC(unsigned int, glDeleteShader, unsigned int);
DECL_GL_FUNC(void, glGetShaderiv, unsigned int, unsigned int, int*);
DECL_GL_FUNC(void, glGetShaderInfoLog, unsigned int, int, int*, char*);
DECL_GL_FUNC(void, glGetProgramiv, unsigned int, unsigned int, int*);
DECL_GL_FUNC(void, glGetProgramInfoLog, unsigned int, int, int*, char*);
DECL_GL_FUNC(void, glGenVertexArrays, int, unsigned int*);
DECL_GL_FUNC(void, glBindVertexArray, unsigned int);
DECL_GL_FUNC(void, glGenBuffers, int, unsigned int*);
DECL_GL_FUNC(void, glBindBuffer, unsigned int, unsigned int);
DECL_GL_FUNC(void, glBufferData, unsigned int, int, const void*, unsigned int);
DECL_GL_FUNC(void, glGenTextures, int, unsigned int*);
DECL_GL_FUNC(void, glDeleteTextures, int, const unsigned int*);
DECL_GL_FUNC(void, glBindTexture, unsigned int, unsigned int);
DECL_GL_FUNC(void, glTexParameteri, unsigned int, unsigned int, int);
DECL_GL_FUNC(void, glTexImage2D, unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
DECL_GL_FUNC(void, glTexSubImage2D, unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
DECL_GL_FUNC(void, glActiveTexture, unsigned int);
DECL_GL_FUNC(void, glViewport, int, int, int, int);
DECL_GL_FUNC(const unsigned char*, glGetString, unsigned int);
DECL_GL_FUNC(void, glUniformMatrix4fv, int, int, unsigned char, const float*);
DECL_GL_FUNC(void, glBufferSubData, unsigned int, int, int, const void*);
DECL_GL_FUNC(void, glPointSize, float);
DECL_GL_FUNC(void, glLineWidth, float);
DECL_GL_FUNC(void, glDisableVertexAttribArray, unsigned int);
DECL_GL_FUNC(void, glDisable, unsigned int);
DECL_GL_FUNC(void, glCopyTexImage2D, unsigned int, int, int, int, int, int, unsigned int, unsigned int);
DECL_GL_FUNC(void, glCopyTexSubImage2D, unsigned int, int, int, int, int, int, int, int);
// FBO functions (OpenGL 3.0+)
DECL_GL_FUNC(void, glGenFramebuffers, int, unsigned int*);
DECL_GL_FUNC(void, glBindFramebuffer, unsigned int, unsigned int);
DECL_GL_FUNC(void, glFramebufferTexture2D, unsigned int, unsigned int, unsigned int, unsigned int, int);
DECL_GL_FUNC(void, glDeleteFramebuffers, int, const unsigned int*);
DECL_GL_FUNC(unsigned int, glCheckFramebufferStatus, unsigned int);

static void LoadGLFunctions() {
    LOAD_GL_FUNC(glClearColor);
    LOAD_GL_FUNC(glClear);
    LOAD_GL_FUNC(glEnable);
    LOAD_GL_FUNC(glBlendFunc);
    LOAD_GL_FUNC(glCreateProgram);
    LOAD_GL_FUNC(glLinkProgram);
    LOAD_GL_FUNC(glUseProgram);
    LOAD_GL_FUNC(glGetUniformLocation);
    LOAD_GL_FUNC(glUniform1f);
    LOAD_GL_FUNC(glUniform1i);
    LOAD_GL_FUNC(glUniform1fv);
    LOAD_GL_FUNC(glUniform2f);
    LOAD_GL_FUNC(glUniform3f);
    LOAD_GL_FUNC(glUniform4f);
    LOAD_GL_FUNC(glUniformMatrix4fv);
    LOAD_GL_FUNC(glVertexAttribPointer);
    LOAD_GL_FUNC(glEnableVertexAttribArray);
    LOAD_GL_FUNC(glDrawArrays);
    LOAD_GL_FUNC(glCreateShader);
    LOAD_GL_FUNC(glShaderSource);
    LOAD_GL_FUNC(glCompileShader);
    LOAD_GL_FUNC(glAttachShader);
    LOAD_GL_FUNC(glDeleteShader);
    LOAD_GL_FUNC(glGetShaderiv);
    LOAD_GL_FUNC(glGetShaderInfoLog);
    LOAD_GL_FUNC(glGetProgramiv);
    LOAD_GL_FUNC(glGetProgramInfoLog);
    LOAD_GL_FUNC(glGenVertexArrays);
    LOAD_GL_FUNC(glBindVertexArray);
    LOAD_GL_FUNC(glGenBuffers);
    LOAD_GL_FUNC(glBindBuffer);
    LOAD_GL_FUNC(glBufferData);
    LOAD_GL_FUNC(glGenTextures);
    LOAD_GL_FUNC(glDeleteTextures);
    LOAD_GL_FUNC(glBindTexture);
    LOAD_GL_FUNC(glTexParameteri);
    LOAD_GL_FUNC(glTexImage2D);
    LOAD_GL_FUNC(glTexSubImage2D);
    LOAD_GL_FUNC(glActiveTexture);
    LOAD_GL_FUNC(glViewport);
    LOAD_GL_FUNC(glGetString);
    LOAD_GL_FUNC(glUniformMatrix4fv);
    LOAD_GL_FUNC(glBufferSubData);
    LOAD_GL_FUNC(glPointSize);
    LOAD_GL_FUNC(glLineWidth);
    LOAD_GL_FUNC(glDisableVertexAttribArray);
    LOAD_GL_FUNC(glDisable);
    LOAD_GL_FUNC(glCopyTexImage2D);
    LOAD_GL_FUNC(glCopyTexSubImage2D);
    // FBO functions
    LOAD_GL_FUNC(glGenFramebuffers);
    LOAD_GL_FUNC(glBindFramebuffer);
    LOAD_GL_FUNC(glFramebufferTexture2D);
    LOAD_GL_FUNC(glDeleteFramebuffers);
    LOAD_GL_FUNC(glCheckFramebufferStatus);
}

// ---- Shader helpers ----
static std::string ReadFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        fprintf(stderr, "[BlackHole] Failed to read shader: %s\n", path);
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool CompileShader(unsigned int shader, const char* label) {
    glCompileShader(shader);
    int ok = 0;
    glGetShaderiv(shader, 0x8B81/*GL_COMPILE_STATUS*/, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        fprintf(stderr, "[BlackHole] Shader compile error (%s):\n%s\n", label, log);
        return false;
    }
    return true;
}

static unsigned int CreateProgram(unsigned int vs, unsigned int fs) {
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int ok = 0;
    glGetProgramiv(prog, 0x8B82/*GL_LINK_STATUS*/, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        fprintf(stderr, "[BlackHole] Program link error:\n%s\n", log);
        return 0;
    }
    return prog;
}

// ---- Fullscreen quad geometry ----
static void CreateQuad(unsigned int* vao, unsigned int* vbo) {
    float verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };
    glGenVertexArrays(1, vao);
    glBindVertexArray(*vao);

    glGenBuffers(1, vbo);
    glBindBuffer(0x8892/*GL_ARRAY_BUFFER*/, *vbo);
    glBufferData(0x8892/*GL_ARRAY_BUFFER*/, sizeof(verts), verts, 0x88E4/*GL_STATIC_DRAW*/);

    glVertexAttribPointer(0, 2, 0x1406/*GL_FLOAT*/, 0, 0, (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

// ---- Particle system (screen-space, aligned with black hole center) ----
static const int MAX_PARTICLES     = 3000;
static const int MAX_TRAIL_LENGTH  = 64;     // max history positions per particle

// ---- Star system (interactive, mouse-driven) ----
enum StarState { STAR_GROWING, STAR_ALIVE, STAR_FADING, STAR_DEAD };
enum StarType  { STAR_O, STAR_B, STAR_A, STAR_F, STAR_G_TYPE, STAR_K, STAR_M };

// Evolution stage (visual type) and evolution sub-state (right-click driven)
enum StarStage { STAGE_NORMAL, STAGE_WHITE_DWARF, STAGE_NEUTRON_STAR, STAGE_BLACK_HOLE };
enum StarEvo   { EVO_IDLE, EVO_EXPANDING, EVO_COLLAPSING, EVO_PRE_SUPERNOVA, EVO_SUPERNOVA, EVO_SETTLE };

// Black hole merger phases
enum MergerPhase { MERGER_IDLE, MERGER_INSPIRAL, MERGER_MERGER, MERGER_RINGDOWN, MERGER_DONE };

struct StarObj {
    float x, y;             // screen UV position [0,1]
    float size;             // current radius (UV)
    float alpha;            // opacity
    float age;              // time since creation
    StarState state;
    StarType  type;
    float baseR, baseG, baseB;  // base color from spectral type
    float colorPhase;       // color shift phase

    // Stellar evolution
    StarStage stage;            // current visual stage
    StarEvo   evo;              // evolution sub-state
    float evoTimer;             // timer within current evo sub-state
    float collapseProgress;     // 0..1 collapse progress (passed to shader, <0 = inactive)
    float supernovaProgress;    // 0..1 supernova progress (<0 = inactive)
    float expandProgress;        // 0..1 first-right-click expansion progress
    float nsRotation;           // neutron star rotation angle (accumulates)
    float magAxisTilt;          // magnetic axis tilt (random per star)
    float bhFormationProgress;  // 0..1 mini black hole formation progress
    float vx, vy;              // velocity (UV/sec) — used by mini BH orbital physics

    // Tidal disruption（恒星潮汐撕裂）
    // Why: 本体形变与能量流必须由同一物理量驱动；历史上二者分别用距离与 age，
    //      互不知情，导致"轨迹与本体分离"。
    // 双量分离（v2.0）：本体形变与剥离物质流是两个不同物理量的函数，不可混用。
    //   tidalAmp   = stretch * p^0.8  → 有界，只负责本体轻微伸长
    //   tidalStrip = p^1.2            → 只负责流的宽度/亮度/色温（尾部主导）
    float tidalProgress;       // p: 剥离进度 [0, 1]，不可逆
    float tidalAmp;            // 本体最大伸长率（有界，CPU 单点计算 → S4 不变量）
    float tidalStrip;          // 剥离物质流强度（流 shader 专用）

    // Black hole merger
    MergerPhase mergerPhase;    // current merger phase
    float mergerTimer;          // timer within current merger phase
    float mergerOrbitAngle;     // current orbit angle around main BH
    float mergerFlash;          // merger flash intensity (0..1)
    float mergerRingdownAmp;    // ringdown oscillation amplitude
    float mergerGWFreq;         // gravitational wave frequency (chirp)
    float mergerBaseSize;       // mini BH size at merger start (for shrink)
    float mergerMainBHOffsetX;  // main BH offset toward mini BH (UV)
    float mergerMainBHOffsetY;  // main BH offset toward mini BH (UV)
    float mergerMainBHStartX;   // main BH start X position when merger triggered
    float mergerMainBHStartY;   // main BH start Y position when merger triggered
};

static const int MAX_STARS = 3;
static StarObj s_stars[MAX_STARS];
static int s_starCount = 0;

// 全局撕裂联动量 L ∈ [0,1]：每帧在恒星循环前清零，循环内取所有恒星的 max(strip)。
// Why: 粒子吸积盘 / 主吸积盘 / 喷流 是全局单例资源，无法按恒星分别调制，
//      因此需要一个帧级的"最大撕裂强度"来驱动它们的联动增益。
static float s_tidalLink = 0.0f;

// Shared collapse-effect state (computed in star update, applied to background next frame)
static float s_evolveShake = 0.0f;     // 0..1 shake intensity (drives pixel offset)
static float s_evolveVignette = 0.0f;  // 0..1 vignette
static float s_evolveTension = 0.0f;   // 0..1 tension lines
static float s_shakeX = 0.0f, s_shakeY = 0.0f;  // current shake pixel offset

// ---- Solar wind particles (emitted from star surface outward) ----
static const int MAX_WIND_PARTICLES = 1500;

struct WindParticle {
    float x, y;           // screen UV [0,1]
    float vx, vy;         // velocity in UV/sec
    float life;
    float maxLife;
    float speed;          // emission speed factor
};

static WindParticle s_windParticles[MAX_WIND_PARTICLES];

static float RandomFloat() {
    return (float)rand() / (float)RAND_MAX;
}

// Emit solar wind particles from a star
static void EmitWindParticles(StarObj& star, float dt, int maxCount) {
    // Emission rate proportional to star size
    float emitRate = star.size * 800.0f; // particles per second
    int toEmit = (int)(emitRate * dt);
    if (toEmit < 1) toEmit = (RandomFloat() < emitRate * dt) ? 1 : 0;
    if (toEmit > 10) toEmit = 10; // cap per frame

    for (int e = 0; e < toEmit; e++) {
        // Find a dead particle slot
        int slot = -1;
        for (int i = 0; i < maxCount; i++) {
            if (s_windParticles[i].life <= 0.0f) { slot = i; break; }
        }
        if (slot < 0) return; // all particles alive

        WindParticle& wp = s_windParticles[slot];
        // Random direction from star surface
        float angle = RandomFloat() * 6.2831853f;
        float emitSpeed = (0.1f + RandomFloat() * 0.2f) * star.size;
        wp.x = star.x + cosf(angle) * star.size * 0.8f;
        wp.y = star.y + sinf(angle) * star.size * 0.8f;
        wp.vx = cosf(angle) * emitSpeed;
        wp.vy = sinf(angle) * emitSpeed;
        wp.life = 1.0f;
        wp.maxLife = 1.5f + RandomFloat() * 2.0f;
        wp.speed = 0.3f + RandomFloat() * 0.7f;
    }
}

// Update solar wind particles
static void UpdateWindParticles(float dt, float bhCx, float bhCy, float bhGravity) {
    for (int i = 0; i < MAX_WIND_PARTICLES; i++) {
        WindParticle& wp = s_windParticles[i];
        if (wp.life <= 0.0f) continue;

        // Move outward
        wp.x += wp.vx * dt;
        wp.y += wp.vy * dt;

        // Slight deceleration
        wp.vx *= (1.0f - 0.3f * dt);
        wp.vy *= (1.0f - 0.3f * dt);

        // Weak attraction toward black hole (if present)
        float dxBh = bhCx - wp.x;
        float dyBh = bhCy - wp.y;
        float distBh = sqrtf(dxBh * dxBh + dyBh * dyBh);
        if (distBh > 0.01f) {
            float pull = bhGravity * 0.0005f / (distBh * distBh + 0.01f);
            wp.vx += dxBh / distBh * pull * dt;
            wp.vy += dyBh / distBh * pull * dt;
        }

        // Drain life
        wp.life -= dt / wp.maxLife;

        // Kill if off screen
        if (wp.x < -0.1f || wp.x > 1.1f || wp.y < -0.1f || wp.y > 1.1f) {
            wp.life = 0.0f;
        }
    }
}

// Initialize star objects to DEAD state
static void InitStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        s_stars[i].state = STAR_DEAD;
        s_stars[i].alpha = 0.0f;
        s_stars[i].stage = STAGE_NORMAL;
        s_stars[i].evo = EVO_IDLE;
        s_stars[i].evoTimer = 0.0f;
        s_stars[i].collapseProgress = -1.0f;
        s_stars[i].supernovaProgress = -1.0f;
        s_stars[i].expandProgress = -1.0f;
        s_stars[i].nsRotation = 0.0f;
        s_stars[i].magAxisTilt = 0.0f;
        s_stars[i].bhFormationProgress = 0.0f;
        s_stars[i].vx = 0.0f;
        s_stars[i].vy = 0.0f;
        s_stars[i].mergerPhase = MERGER_IDLE;
        s_stars[i].mergerTimer = 0.0f;
        s_stars[i].mergerOrbitAngle = 0.0f;
        s_stars[i].mergerFlash = 0.0f;
        s_stars[i].mergerRingdownAmp = 0.0f;
        s_stars[i].mergerGWFreq = 0.0f;
        s_stars[i].mergerBaseSize = 0.0f;
        s_stars[i].mergerMainBHOffsetX = 0.0f;
        s_stars[i].mergerMainBHOffsetY = 0.0f;
        s_stars[i].mergerMainBHStartX = 0.5f;
        s_stars[i].mergerMainBHStartY = 0.5f;
    }
    s_starCount = 0;
}

// Spectral type colors (approximate blackbody)
static void GetStarTypeColor(StarType type, float* r, float* g, float* b) {
    switch (type) {
        case STAR_O:       *r = 0.45f; *g = 0.55f; *b = 1.0f;  break; // deep blue giant
        case STAR_B:       *r = 0.6f;  *g = 0.7f;  *b = 1.0f;  break; // blue-white
        case STAR_A:       *r = 0.85f; *g = 0.88f; *b = 1.0f;  break; // white
        case STAR_F:       *r = 1.0f;  *g = 0.96f; *b = 0.85f; break; // yellow-white
        case STAR_G_TYPE:  *r = 1.0f;  *g = 0.85f; *b = 0.4f;  break; // yellow (Sun-like)
        case STAR_K:       *r = 1.0f;  *g = 0.6f;  *b = 0.3f;  break; // orange
        case STAR_M:       *r = 1.0f;  *g = 0.35f; *b = 0.15f; break; // red
    }
}

static StarType RandomStarType() {
    // Weighted distribution for visual variety:
    // O (blue giant) 12%, B (blue-white) 14%, A (white) 15%,
    // F (yellow-white) 14%, G (yellow) 15%, K (orange) 15%, M (red) 15%
    int r = rand() % 100;
    if (r < 12) return STAR_O;
    if (r < 26) return STAR_B;
    if (r < 41) return STAR_A;
    if (r < 55) return STAR_F;
    if (r < 70) return STAR_G_TYPE;
    if (r < 85) return STAR_K;
    return STAR_M;
}

struct Particle {
    float x, y;               // screen UV [0,1]
    float vx, vy;             // UV units per second (also used as polar state)
    float life;
    float maxLife;
    float thickness;          // per-particle trail thickness variation [0.5, 1.5]
    float streamWidth;        // per-particle base stream width variation
    float trailX[MAX_TRAIL_LENGTH];
    float trailY[MAX_TRAIL_LENGTH];
    int   trailCount;
};

static Particle s_particles[MAX_PARTICLES];
static int s_activeParticleCount = 0;
static int s_targetParticleCount = 0;    // user-selected count (may exceed MAX)
static int s_trailLength = 40;           // actual trail length used [5, MAX_TRAIL_LENGTH]
static bool s_particlesInitialized = false;

static void InitParticle(Particle& p) {
    const float cx = 0.5f, cy = 0.5f;

    // Each particle stores polar state in vx (angle) and vy (radius)
    p.vx = RandomFloat() * 6.2831853f;          // angle
    p.vy = 0.35f + RandomFloat() * 0.40f;       // radius [0.35, 0.75]

    p.x = cx + cosf(p.vx) * p.vy;
    p.y = cy + sinf(p.vx) * p.vy;

    p.life = 1.0f;
    p.maxLife = 8.0f + RandomFloat() * 12.0f;
    p.thickness = 0.5f + RandomFloat() * 1.0f; // 0.5 ~ 1.5 variation
    p.streamWidth = 0.4f + RandomFloat() * 1.2f; // some streams thin, some thick

    p.trailCount = 0;
    for (int i = 0; i < MAX_TRAIL_LENGTH; i++) {
        p.trailX[i] = p.x;
        p.trailY[i] = p.y;
    }
}

// Lissajous helper matching blackhole.glsl
static void Lissa(float t, float* outX, float* outY) {
    *outX = 0.75f * sinf(t * 0.37f) + 0.25f * sinf(t * 0.83f + 1.0f);
    *outY = 0.70f * sinf(t * 0.54f + 2.1f) + 0.30f * sinf(t * 1.07f);
}

// Reproduce the black hole center in CPU so particles can follow it.
// This mirrors the MODE_DEMO branch in blackhole.glsl (SIZE_MODE == MODE_DEMO).
// outShadowR (可选)：屏幕阴影半径 rhT（aspect 校正 UV），= blackhole.glsl 内的 rh = HOLE_RADIUS*sz。
// Why: 能量流的螺旋必须钉在真实吸积盘上。流 shader 自行推导尺度会与 blackhole.glsl 漂移 2.0~2.6 倍
//      （历史 bug：流悬在盘外并被裁掉，观感像"被黑洞外围引力场挡住"）。这里把权威值暴露给 CPU，
//      CPU 再换算成盘内/外半径以 uniform 广播，保证两侧不可能各自漂移。
static void ComputeBlackHoleCenter(float t, float aspect, bool demoMode, float fixedLevel, bool fixedSize,
                                   float* outCx, float* outCy, float* outShadowR) {
    const float HOLE_RADIUS   = 0.0200f;
    const float TOKEN_EASE    = 1.0f;
    const float TOKEN_AREA_MIN = 0.0100f;
    const float TOKEN_AREA_MAX = 0.5000f;
    const float TOKEN_HOME_X  = 0.9600f;
    const float TOKEN_HOME_Y  = 0.0400f;
    const float TOKEN_REACH   = 1.0f;
    const float TOKEN_CALM    = 0.0400f;
    const float TOKEN_RUSH    = 1.1000f;
    const float DEMO_SEC      = 42.0f;
    const float DEMO_GROW_SEC = 40.0f;
    const float WORK_AREA     = 0.0f;
    const float PI            = 3.1415927f;

    float lvl;
    if (demoMode) {
        lvl = fminf(fmodf(t, DEMO_SEC) / DEMO_GROW_SEC, 1.0f);
    } else if (fixedSize) {
        lvl = fixedLevel;
    } else {
        lvl = 1.0f;
    }

    float g = powf(fmaxf(0.0f, fminf(1.0f, lvl)), TOKEN_EASE);
    float I = 0.10f + (1.0f - 0.10f) * g;

    float rhMin = sqrtf(TOKEN_AREA_MIN * aspect / PI);
    float rhMax = sqrtf(TOKEN_AREA_MAX * aspect / PI);
    float rhT   = (rhMin + (rhMax - rhMin) * g) * (HOLE_RADIUS / 0.08f);

    float marg = fminf(rhT * (1.45f - 0.55f * g), 0.5f * (1.0f - WORK_AREA - 0.03f));
    float xPad = marg / aspect;

    float fullLoX = xPad;
    float fullLoY = marg;
    float fullHiX = fmaxf(0.5f, 1.0f - xPad);
    float fullHiY = fmaxf(marg, 1.0f - (WORK_AREA + 0.03f + marg));

    float cornerX = fmaxf(fullLoX, fminf(TOKEN_HOME_X, fullHiX));
    float cornerY = fmaxf(fullLoY, fminf(TOKEN_HOME_Y, fullHiY));

    float reach = 0.06f + (fmaxf(TOKEN_REACH, 0.06f) - 0.06f) * g;

    float loX = cornerX + (fullLoX - cornerX) * reach;
    float loY = fullLoY;
    float hiX = fullHiX;
    float hiY = cornerY + (fullHiY - cornerY) * reach;

    float roomX = fmaxf((hiX - loX) * 0.5f, 0.0f);
    float roomY = fmaxf((hiY - loY) * 0.5f, 0.0f);

    float wobAmpX = fminf(0.010f + 0.030f * g, fmaxf(roomX * 0.35f, 0.006f));
    float wobAmpY = fminf(0.010f + 0.030f * g, fmaxf(roomY * 0.35f, 0.006f));

    float ampEffX = fmaxf(roomX - wobAmpX, 0.0f);
    float ampEffY = fmaxf(roomY - wobAmpY, 0.0f);

    float calmX, calmY, rushX, rushY;
    Lissa(t * TOKEN_CALM, &calmX, &calmY);
    Lissa(t * TOKEN_RUSH, &rushX, &rushY);

    float wanderX = calmX + (rushX - calmX) * g;
    float wanderY = calmY + (rushY - calmY) * g;

    *outCx = (loX + hiX) * 0.5f + wanderX * ampEffX + wobAmpX * cosf(t * 0.8f);
    *outCy = (loY + hiY) * 0.5f + wanderY * ampEffY + wobAmpY * sinf(t * 1.0f);
    if (outShadowR) *outShadowR = rhT;
}

static void UpdateParticles(float dt, float gravity, float spiral, float cx, float cy, float spinA) {

    for (int i = 0; i < s_activeParticleCount; i++) {
        Particle& p = s_particles[i];

        // Offset from black hole center
        float dx = p.x - cx;
        float dy = p.y - cy;
        float radius = sqrtf(dx * dx + dy * dy);
        float angle  = atan2f(dy, dx);

        radius = fmaxf(radius, 0.02f);

        // Base Kepler orbit: angular velocity proportional to r^{-3/2}
        float angularSpeed = spiral * 0.25f / sqrtf(radius);

        // Inward sink: faster near center, scaled by gravity param
        float inwardSpeed = gravity * 0.015f / radius;

        // Kerr frame-dragging: prograde angular boost near the black hole
        // In Boyer-Lindquist coordinates, the frame-dragging angular velocity is:
        //   ω = 2Mar / Σ²  (approximated here for screen-space)
        // Particles are dragged in the prograde direction, stronger at small r
        if (spinA > 0.001f) {
            float rHorizon = 0.5f + sqrtf(std::max(0.25f - spinA * spinA, 0.0f));
            float rSafe = std::max(radius - rHorizon, 0.02f);

            // Frame-dragging: additional prograde angular velocity
            // Stronger closer to the horizon, scaled by spin parameter
            float dragOmega = spinA * 0.08f / (rSafe * rSafe);
            angularSpeed += dragOmega;

            // Kerr ISCO asymmetry: prograde particles orbit closer
            // Prograde direction = positive angle change
            float iscoPro = 3.0f + sqrtf(std::max(3.0f * spinA * spinA, 0.0f))
                          - sqrtf(std::max((3.0f - 1.0f) * (3.0f + 1.0f + 2.0f * sqrtf(std::max(3.0f * spinA * spinA, 0.0f))), 0.0f));
            float iscoRet = 3.0f + sqrtf(std::max(3.0f * spinA * spinA, 0.0f))
                          + sqrtf(std::max(2.0f * (3.0f + sqrtf(std::max(3.0f * spinA * spinA, 0.0f))), 0.0f));

            // Inward speed slightly enhanced for prograde, reduced for retrograde
            // Simple approximation: always use prograde ISCO for infall rate
            float iscoScale = std::max(iscoPro / 6.0f, 0.15f);  // normalized to Schwarzschild ISCO=6
            inwardSpeed *= (0.5f + 0.5f * iscoScale);
        }

        angle  += angularSpeed * dt;
        radius -= inwardSpeed * dt;

        // Add tiny wobble for organic look
        angle += (RandomFloat() - 0.5f) * 0.02f * dt;

        p.x = cx + cosf(angle) * radius;
        p.y = cy + sinf(angle) * radius;

        // Keep polar state in vx/vy for continuity
        p.vx = angle;
        p.vy = radius;

        // Record trail history
        for (int j = MAX_TRAIL_LENGTH - 1; j > 0; j--) {
            p.trailX[j] = p.trailX[j - 1];
            p.trailY[j] = p.trailY[j - 1];
        }
        p.trailX[0] = p.x;
        p.trailY[0] = p.y;
        if (p.trailCount < s_trailLength) p.trailCount++;

        // Fade faster near center
        float lifeDrain = dt / p.maxLife;
        if (radius < 0.05f) lifeDrain *= 3.0f;
        p.life -= lifeDrain;

        // Respawn dead or swallowed particles
        if (p.life <= 0.0f || radius < 0.015f) {
            // Spawn at a new random position relative to current black hole center
            float spawnAngle = RandomFloat() * 6.2831853f;
            float spawnRadius = 0.35f + RandomFloat() * 0.35f;

            // Kerr: prograde side (where frame-dragging helps) spawns closer
            // Retrograde side spawns farther out — creates asymmetric density
            if (spinA > 0.001f) {
                float progradeBias = spinA * 0.15f;  // asymmetry strength
                // Prograde = positive angle direction
                float progradeSign = sinf(spawnAngle);  // simplified
                spawnRadius -= progradeSign * progradeBias * spawnRadius;
                spawnRadius = std::max(spawnRadius, 0.05f);
            }

            p.x = cx + cosf(spawnAngle) * spawnRadius;
            p.y = cy + sinf(spawnAngle) * spawnRadius;
            p.vx = spawnAngle;
            p.vy = spawnRadius;
            p.life = 1.0f;
            p.maxLife = 8.0f + RandomFloat() * 12.0f;
            p.thickness = 0.5f + RandomFloat() * 1.0f;
            p.streamWidth = 0.4f + RandomFloat() * 1.2f;
            for (int k = 0; k < MAX_TRAIL_LENGTH; k++) {
                p.trailX[k] = p.x;
                p.trailY[k] = p.y;
            }
            p.trailCount = s_trailLength;
        }
    }
}

// ---- Simple 4x4 matrix math (column-major for OpenGL) ----
static void Mat4Identity(float* m) {
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void Mat4Perspective(float* m, float fov, float aspect, float nearP, float farP) {
    float f = 1.0f / tanf(fov * 0.5f);
    memset(m, 0, 64);
    m[0]  = f / aspect;
    m[5]  = f;
    m[10] = (farP + nearP) / (nearP - farP);
    m[11] = -1.0f;
    m[14] = (2.0f * farP * nearP) / (nearP - farP);
}

static void Mat4LookAt(float* m, float ex, float ey, float ez,
                        float cx, float cy, float cz,
                        float ux, float uy, float uz) {
    // glm::lookAt implementation reference:
    // f = normalize(center - eye)       // forward (camera looks toward center)
    // s = normalize(cross(f, up))       // right
    // u = cross(s, f)                   // true up
    //
    // Matrix layout (column-major, m[col*4 + row]):
    // glm uses mat[col][row] where mat[0] is column 0, mat[0][0] is row 0 of column 0
    // In array: m[col*4 + row]
    //
    // Result[0][0] = s.x → m[0]
    // Result[1][0] = s.y → m[4]
    // Result[2][0] = s.z → m[8]
    // Result[0][1] = u.x → m[1]
    // Result[1][1] = u.y → m[5]
    // Result[2][1] = u.z → m[9]
    // Result[0][2] = -f.x → m[2]
    // Result[1][2] = -f.y → m[6]
    // Result[2][2] = -f.z → m[10]
    // Result[3][0] = -dot(s,eye) → m[12]
    // Result[3][1] = -dot(u,eye) → m[13]
    // Result[3][2] =  dot(f,eye) → m[14]

    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float fl = sqrtf(fx*fx + fy*fy + fz*fz);
    fx /= fl; fy /= fl; fz /= fl;

    float sx = fy*uz - fz*uy, sy = fz*ux - fx*uz, sz = fx*uy - fy*ux;
    float sl = sqrtf(sx*sx + sy*sy + sz*sz);
    if (sl > 0.0001f) { sx /= sl; sy /= sl; sz /= sl; }

    float ux2 = sy*fz - sz*fy, uy2 = sz*fx - sx*fz, uz2 = sx*fy - sy*fx;

    m[0]  = sx;     m[1]  = ux2;   m[2]  = -fx;   m[3]  = 0.0f;
    m[4]  = sy;     m[5]  = uy2;   m[6]  = -fy;   m[7]  = 0.0f;
    m[8]  = sz;     m[9]  = uz2;   m[10] = -fz;   m[11] = 0.0f;
    m[12] = -(sx*ex + sy*ey + sz*ez);
    m[13] = -(ux2*ex + uy2*ey + uz2*ez);
    m[14] = (fx*ex + fy*ey + fz*ez);
    m[15] = 1.0f;
}

static void Mat4Multiply(float* out, const float* a, const float* b) {
    float tmp[16];
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            tmp[col*4+row] =
                a[0*4+row]*b[col*4+0] +
                a[1*4+row]*b[col*4+1] +
                a[2*4+row]*b[col*4+2] +
                a[3*4+row]*b[col*4+3];
        }
    }
    memcpy(out, tmp, 64);
}

// ---- Entry point ----
// 崩溃兜底：仅做最小工作（还原桌面图标），随后终止进程，避免图标残留在中心。
static LONG WINAPI DesktopIcons_UnhandledFilter(EXCEPTION_POINTERS* /*info*/) {
    DesktopIcons_Shutdown();
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv) {
    printf("[BlackHole] Starting...\n");

    // 统一退出/异常还原（Q1：退出与崩溃都还原图标原位）
    atexit(DesktopIcons_Shutdown);
    SetUnhandledExceptionFilter(DesktopIcons_UnhandledFilter);

    // Initialize config
    BlackholeConfig config = {};
    Config_SetDefaults(&config);
    Config_InitBuiltInPresets(&config);
    Config_Load(&config, "blackhole_config.ini");

    // Determine primary monitor resolution
    SetProcessDPIAware();
    HMONITOR hmon = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hmon, &mi);
    int screenW = mi.rcMonitor.right - mi.rcMonitor.left;
    int screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;
    printf("[BlackHole] Monitor resolution: %dx%d\n", screenW, screenH);

    // Init Win32 + WGL
    Win32GL wgl = {};
    if (!Win32GL_Init(&wgl, hmon, screenW, screenH)) {
        fprintf(stderr, "[BlackHole] Win32GL_Init failed\n");
        return 1;
    }
    printf("[BlackHole] Win32GL initialized OK\n");

    // Load GL functions
    LoadGLFunctions();
    printf("[BlackHole] GL functions loaded\n");

    if (!glGetString) {
        fprintf(stderr, "[BlackHole] glGetString not loaded - GL context may be invalid\n");
        Win32GL_Shutdown(&wgl);
        return 1;
    }

    printf("[BlackHole] GL Version: %s\n", (const char*)glGetString(0x1F02/*GL_VERSION*/));
    printf("[BlackHole] GLSL Version: %s\n", (const char*)glGetString(0x8B8C/*GL_SHADING_LANGUAGE_VERSION*/));
    printf("[BlackHole] Renderer: %s\n", (const char*)glGetString(0x1F01/*GL_RENDERER*/));

    // Initialize ImGui
    GUI_Init(&wgl);
    printf("[BlackHole] ImGui initialized\n");

    // 窗口吞噬模块（shader 编译失败仍启用：Trigger 走纯 SW_HIDE 降级路径）
    WindowDevour_Init(wgl.hwnd);

    // Read vertex shader
    std::string vertSrc = ReadFile("shaders/vert.glsl");
    if (vertSrc.empty()) {
        fprintf(stderr, "[BlackHole] Failed to load vertex shader\n");
        GUI_Shutdown();
        Win32GL_Shutdown(&wgl);
        return 1;
    }

    // Read fragment shader parts
    std::string fragHeader = ReadFile("shaders/frag_desktop_header.glsl");
    std::string fragBody   = ReadFile("blackhole.glsl");
    if (fragHeader.empty() || fragBody.empty()) {
        fprintf(stderr, "[BlackHole] Failed to load fragment shader parts\n");
        GUI_Shutdown();
        Win32GL_Shutdown(&wgl);
        return 1;
    }

    // Source patching
    auto patchConst = [](std::string& src, const char* name, const char* uniform) {
        // Find "const float <name>" then skip whitespace to '=', then to ';'
        std::string prefix = std::string("const float ") + name;
        size_t pos = src.find(prefix);
        if (pos == std::string::npos) return;
        size_t eqPos = src.find('=', pos + prefix.size());
        size_t semiPos = src.find(';', eqPos);
        if (eqPos == std::string::npos || semiPos == std::string::npos) return;
        // Extract default value between '=' and ';', trimming whitespace
        size_t valStart = eqPos + 1;
        while (valStart < semiPos && (src[valStart] == ' ' || src[valStart] == '\t')) valStart++;
        size_t valEnd = semiPos;
        while (valEnd > valStart && (src[valEnd-1] == ' ' || src[valEnd-1] == '\t')) valEnd--;
        std::string defaultVal = src.substr(valStart, valEnd - valStart);
        std::string replacement = "float " + std::string(name) + " = " + uniform + " >= 0.0 ? " + uniform + " : " + defaultVal + ";";
        src.replace(pos, semiPos - pos + 1, replacement);
        printf("[BlackHole] Patched %s: default=%s\n", name, defaultVal.c_str());
    };

    // SIZE_MODE remains MODE_DEMO. Demo-tour cycling is controlled at runtime via uDemoMode uniform in blackhole.glsl.
    {
        size_t pos = fragBody.find("#define SIZE_MODE");
        if (pos != std::string::npos) {
            size_t eol = fragBody.find('\n', pos);
            fragBody.replace(pos, eol - pos, "#define SIZE_MODE MODE_DEMO");
        }
    }

    // Patch WORK_AREA to 0
    {
        size_t pos = fragBody.find("const float WORK_AREA");
        if (pos != std::string::npos) {
            size_t eqPos = fragBody.find('=', pos);
            size_t semiPos = fragBody.find(';', eqPos);
            fragBody.replace(eqPos + 2, semiPos - eqPos - 2, "0.0000");
        }
    }

    // Patch born progress
    {
        const char* marker = "float rh = HOLE_RADIUS * sz;";
        size_t pos = fragBody.find(marker);
        if (pos != std::string::npos) {
            std::string insert = "\n    sz *= uBornProgress;\n    ";
            fragBody.insert(pos, insert);
        }
    }

    // Patch roaming range
    {
        size_t pos = fragBody.find("vec2  fullLo = vec2(min(xPad, 0.5), marg)");
        if (pos != std::string::npos) {
            size_t eol = fragBody.find(';', pos);
            fragBody.replace(pos, eol - pos + 1, "vec2  fullLo = vec2(xPad, marg)");
        }
    }

    // Patch 7 const values to be uniform-overridable
    patchConst(fragBody, "HOLE_RADIUS", "uHoleRadius");
    patchConst(fragBody, "DISK_GAIN", "uDiskGain");
    patchConst(fragBody, "DISK_TEMP", "uDiskTemp");
    patchConst(fragBody, "EXPOSURE", "uExposure");
    patchConst(fragBody, "DRIFT_SPEED", "uSpeed");
    patchConst(fragBody, "STAR_GAIN", "uStarGain");
    patchConst(fragBody, "DISK_INCL", "uDiskIncl");
    patchConst(fragBody, "LENS_DEPTH", "uLensDepth");
    patchConst(fragBody, "DILATION_MIN", "uDilationMin");

    // Patch advanced disk parameters
    patchConst(fragBody, "DISK_ROLL", "uDiskRoll");
    patchConst(fragBody, "DISK_INNER", "uDiskInner");
    patchConst(fragBody, "DISK_OUTER", "uDiskOuter");
    patchConst(fragBody, "DISK_OPACITY", "uDiskOpac");
    patchConst(fragBody, "DOPPLER_MIX", "uDopplerMix");
    patchConst(fragBody, "DISK_BEAM", "uDiskBeam");
    patchConst(fragBody, "DISK_CONTRAST", "uDiskContrast");
    patchConst(fragBody, "DISK_WIND", "uDiskWind");
    patchConst(fragBody, "DISK_SPEED", "uDiskSpeed");

    // Patch const struct initializers that use the patched variables
    {
        size_t pos = fragBody.find("const DiskLook LOOK_DEFAULT");
        if (pos != std::string::npos) {
            fragBody.replace(pos, 6, ""); // remove "const " (6 chars including space)
            printf("[BlackHole] Patched LOOK_DEFAULT: removed 'const '\n");
        } else {
            printf("[BlackHole] Warning: LOOK_DEFAULT not found\n");
        }
    }

    std::string fragSrc = fragHeader + "\n" + fragBody;

    // Compile shaders
    const char* vSrc = vertSrc.c_str();
    unsigned int vs = glCreateShader(0x8B31/*GL_VERTEX_SHADER*/);
    glShaderSource(vs, 1, &vSrc, nullptr);
    if (!CompileShader(vs, "vertex")) { GUI_Shutdown(); Win32GL_Shutdown(&wgl); return 1; }

    const char* fSrc = fragSrc.c_str();
    unsigned int fs = glCreateShader(0x8B30/*GL_FRAGMENT_SHADER*/);
    glShaderSource(fs, 1, &fSrc, nullptr);
    if (!CompileShader(fs, "fragment")) { GUI_Shutdown(); Win32GL_Shutdown(&wgl); return 1; }

    unsigned int program = CreateProgram(vs, fs);
    if (!program) { GUI_Shutdown(); Win32GL_Shutdown(&wgl); return 1; }
    glDeleteShader(vs);
    glDeleteShader(fs);

    printf("[BlackHole] Shader program linked successfully\n");

    // ---- Compile particle shader program ----
    std::string pVertSrc = ReadFile("shaders/particle_vert.glsl");
    std::string pFragSrc = ReadFile("shaders/particle_frag.glsl");
    unsigned int particleProgram = 0;
    int puParticleColor = -1;
    int puPointSize = -1;
    int puBHCenter = -1;
    int puBHRadius = -1;
    int puBHLensStrength = -1;
    int puSpinParamParticle = -1;
    int puColorGradientParticle = -1;

    printf("[BlackHole] Particle shaders: vert=%zu bytes, frag=%zu bytes\n", pVertSrc.size(), pFragSrc.size());

    if (!pVertSrc.empty() && !pFragSrc.empty()) {
        const char* pvSrc = pVertSrc.c_str();
        unsigned int pvs = glCreateShader(0x8B31/*GL_VERTEX_SHADER*/);
        glShaderSource(pvs, 1, &pvSrc, nullptr);
        bool pvOk = CompileShader(pvs, "particle_vertex");

        const char* pfSrc = pFragSrc.c_str();
        unsigned int pfs = glCreateShader(0x8B30/*GL_FRAGMENT_SHADER*/);
        glShaderSource(pfs, 1, &pfSrc, nullptr);
        bool pfOk = CompileShader(pfs, "particle_fragment");

        if (pvOk && pfOk) {
            particleProgram = CreateProgram(pvs, pfs);
            if (particleProgram) {
                puParticleColor = glGetUniformLocation(particleProgram, "uParticleColor");
                puPointSize     = glGetUniformLocation(particleProgram, "uPointSize");
                puBHCenter      = glGetUniformLocation(particleProgram, "uBHCenter");
                puBHRadius      = glGetUniformLocation(particleProgram, "uBHRadius");
                puBHLensStrength = glGetUniformLocation(particleProgram, "uBHLensStrength");
                puSpinParamParticle = glGetUniformLocation(particleProgram, "uSpinParam");
                puColorGradientParticle = glGetUniformLocation(particleProgram, "uColorGradient");
                printf("[BlackHole] Particle shader program linked OK (uParticleColor=%d, uPointSize=%d, uBHCenter=%d, uBHRadius=%d, uBHLensStrength=%d, uSpinParam=%d, uColorGradient=%d)\n",
                       puParticleColor, puPointSize, puBHCenter, puBHRadius, puBHLensStrength, puSpinParamParticle, puColorGradientParticle);
            }
        }
        glDeleteShader(pvs);
        glDeleteShader(pfs);
    } else {
        printf("[BlackHole] Warning: particle shaders not found, particle system disabled\n");
    }

    // ---- Compile star shader program ----
    std::string sVertSrc = ReadFile("shaders/star_vert.glsl");
    std::string sFragSrc = ReadFile("shaders/star_frag.glsl");
    unsigned int starProgram = 0;
    int suStarPos = -1, suStarSize = -1, suStarAlpha = -1;
    int suStarColor = -1, suCoronaSize = -1, suTime = -1;
    int suCoronaBright = -1, suFlareChance = -1, suResolution = -1;
    int suBHPos = -1;
    int suTidalAmp = -1;
    int suStarTex = -1;
    // 分层体积气体材质参数（T3~T5）：见 gui_config.h 的 starGasOpacity 等字段
    int suGasOpacity = -1, suChromoScale = -1, suCoronaFalloff = -1;
    int suStreakAmount = -1, suStreakSpeed = -1;
    int suTidalStrip = -1, suTidalBodyFade = -1, suTidalProgress = -1;
    // Evolution uniforms
    int suStarStage = -1, suPassMode = -1;
    int suCollapseProgress = -1, suSupernovaProgress = -1, suExpandProgress = -1;
    int suCollapseTension = -1;
    int suWDBright = -1, suWDCrystal = -1;
    int suNSRotation = -1, suNSMagAxisTilt = -1, suNSJetLen = -1, suNSJetBright = -1, suNSPulseRate = -1;
    int suBHFormationProgress = -1;
    int suFBTexture = -1, suMiniBHLensStrength = -1, suMiniBHRingBright = -1;
    int suDualBHActive = -1, suGWRippleStrength = -1, suAccretionBridgeBright = -1;
    int suMiniBHDiskIncl = -1;

    printf("[BlackHole] Star shaders: vert=%zu bytes, frag=%zu bytes\n", sVertSrc.size(), sFragSrc.size());

    if (!sVertSrc.empty() && !sFragSrc.empty()) {
        const char* svSrc = sVertSrc.c_str();
        unsigned int svs = glCreateShader(0x8B31/*GL_VERTEX_SHADER*/);
        glShaderSource(svs, 1, &svSrc, nullptr);
        bool svOk = CompileShader(svs, "star_vertex");

        const char* sfSrc = sFragSrc.c_str();
        unsigned int sfs = glCreateShader(0x8B30/*GL_FRAGMENT_SHADER*/);
        glShaderSource(sfs, 1, &sfSrc, nullptr);
        bool sfOk = CompileShader(sfs, "star_fragment");

        if (svOk && sfOk) {
            starProgram = CreateProgram(svs, sfs);
            if (starProgram) {
                suStarPos      = glGetUniformLocation(starProgram, "uStarPos");
                suStarSize     = glGetUniformLocation(starProgram, "uStarSize");
                suStarAlpha    = glGetUniformLocation(starProgram, "uStarAlpha");
                suStarColor    = glGetUniformLocation(starProgram, "uStarColor");
                suTime         = glGetUniformLocation(starProgram, "uTime");
                suCoronaSize   = glGetUniformLocation(starProgram, "uCoronaSize");
                suCoronaBright = glGetUniformLocation(starProgram, "uCoronaBright");
                suFlareChance  = glGetUniformLocation(starProgram, "uFlareChance");
                suResolution   = glGetUniformLocation(starProgram, "uResolution");
                suBHPos        = glGetUniformLocation(starProgram, "uBHPos");
                suTidalAmp     = glGetUniformLocation(starProgram, "uTidalAmp");
                suStarTex      = glGetUniformLocation(starProgram, "uStarTex");
                suGasOpacity      = glGetUniformLocation(starProgram, "uGasOpacity");
                suChromoScale     = glGetUniformLocation(starProgram, "uChromoScale");
                suCoronaFalloff   = glGetUniformLocation(starProgram, "uCoronaFalloff");
                suStreakAmount    = glGetUniformLocation(starProgram, "uStreakAmount");
                suStreakSpeed     = glGetUniformLocation(starProgram, "uStreakSpeed");
                suTidalStrip      = glGetUniformLocation(starProgram, "uTidalStrip");
                suTidalBodyFade   = glGetUniformLocation(starProgram, "uTidalBodyFade");
                suTidalProgress   = glGetUniformLocation(starProgram, "uTidalProgress");
                // Evolution uniforms
                suStarStage         = glGetUniformLocation(starProgram, "uStarStage");
                suPassMode          = glGetUniformLocation(starProgram, "uPassMode");
                suCollapseProgress  = glGetUniformLocation(starProgram, "uCollapseProgress");
                suSupernovaProgress = glGetUniformLocation(starProgram, "uSupernovaProgress");
                suExpandProgress     = glGetUniformLocation(starProgram, "uExpandProgress");
                suCollapseTension   = glGetUniformLocation(starProgram, "uCollapseTension");
                suWDBright          = glGetUniformLocation(starProgram, "uWDBright");
                suWDCrystal         = glGetUniformLocation(starProgram, "uWDCrystal");
                suNSRotation        = glGetUniformLocation(starProgram, "uNSRotation");
                suNSMagAxisTilt     = glGetUniformLocation(starProgram, "uNSMagAxisTilt");
                suNSJetLen          = glGetUniformLocation(starProgram, "uNSJetLen");
                suNSJetBright       = glGetUniformLocation(starProgram, "uNSJetBright");
                suNSPulseRate       = glGetUniformLocation(starProgram, "uNSPulseRate");
                suBHFormationProgress = glGetUniformLocation(starProgram, "uBHFormationProgress");
                suFBTexture             = glGetUniformLocation(starProgram, "uFBTexture");
                suMiniBHLensStrength    = glGetUniformLocation(starProgram, "uMiniBHLensStrength");
                suMiniBHRingBright      = glGetUniformLocation(starProgram, "uMiniBHRingBright");
                suDualBHActive          = glGetUniformLocation(starProgram, "uDualBHActive");
                suGWRippleStrength      = glGetUniformLocation(starProgram, "uGWRippleStrength");
                suAccretionBridgeBright = glGetUniformLocation(starProgram, "uAccretionBridgeBright");
                suMiniBHDiskIncl       = glGetUniformLocation(starProgram, "uMiniBHDiskIncl");
                printf("[BlackHole] Star shader program linked OK\n");
            }
        }
        glDeleteShader(svs);
        glDeleteShader(sfs);
    } else {
        printf("[BlackHole] Warning: star shaders not found, star system disabled\n");
    }

    // ---- Load sun texture (try real image first, fallback to procedural) ----
    StarTexture sunTex = StarTexture_LoadFromFile("texture/sun_red.png");
    if (!sunTex.valid) {
        printf("[BlackHole] Real texture not found, generating procedural sun texture\n");
        sunTex = StarTexture_GenerateProcedural(512);
    }
    if (!sunTex.valid) {
        printf("[BlackHole] Warning: sun texture generation failed\n");
    }

    // ---- Compile energy stream shader program ----
    std::string eVertSrc = ReadFile("shaders/energy_stream_vert.glsl");
    std::string eFragSrc = ReadFile("shaders/energy_stream_frag.glsl");
    unsigned int energyProgram = 0;
    int euStarPos = -1, euBHPos = -1, euStreamIntensity = -1;
    int euTime = -1, euResolution = -1;
    int euStreamWidth = -1, euSpiralTurns = -1, euStreamColor = -1;
    int euStarAge = -1;
    int euTidalProgress = -1, euTidalAmp = -1, euStarSize = -1, euTidalStrip = -1;
    // 真实吸积盘内/外径（aspect 校正 UV）：CPU 用权威 rhT 换算后广播，消除 shader 内重复推导（T1/D4）
    int euDiskInnerUV = -1, euDiskOuterUV = -1;

    printf("[BlackHole] Energy stream shaders: vert=%zu bytes, frag=%zu bytes\n", eVertSrc.size(), eFragSrc.size());

    if (!eVertSrc.empty() && !eFragSrc.empty()) {
        const char* evSrc = eVertSrc.c_str();
        unsigned int evs = glCreateShader(0x8B31/*GL_VERTEX_SHADER*/);
        glShaderSource(evs, 1, &evSrc, nullptr);
        bool evOk = CompileShader(evs, "energy_vertex");

        const char* efSrc = eFragSrc.c_str();
        unsigned int efs = glCreateShader(0x8B30/*GL_FRAGMENT_SHADER*/);
        glShaderSource(efs, 1, &efSrc, nullptr);
        bool efOk = CompileShader(efs, "energy_fragment");

        if (evOk && efOk) {
            energyProgram = CreateProgram(evs, efs);
            if (energyProgram) {
                euStarPos         = glGetUniformLocation(energyProgram, "uStarPos");
                euBHPos           = glGetUniformLocation(energyProgram, "uBHPos");
                euStreamIntensity = glGetUniformLocation(energyProgram, "uStreamIntensity");
                euTime            = glGetUniformLocation(energyProgram, "uTime");
                euResolution      = glGetUniformLocation(energyProgram, "uResolution");
                euStreamWidth     = glGetUniformLocation(energyProgram, "uStreamWidth");
                euSpiralTurns     = glGetUniformLocation(energyProgram, "uSpiralTurns");
                euStreamColor     = glGetUniformLocation(energyProgram, "uStreamColor");
                euStarAge         = glGetUniformLocation(energyProgram, "uStarAge");
                euTidalProgress   = glGetUniformLocation(energyProgram, "uTidalProgress");
                euTidalAmp        = glGetUniformLocation(energyProgram, "uTidalAmp");
                euStarSize        = glGetUniformLocation(energyProgram, "uStarSize");
                euTidalStrip      = glGetUniformLocation(energyProgram, "uTidalStrip");
                euDiskInnerUV     = glGetUniformLocation(energyProgram, "uDiskInnerUV");
                euDiskOuterUV     = glGetUniformLocation(energyProgram, "uDiskOuterUV");
                printf("[BlackHole] Energy stream shader program linked OK\n");
            }
        }
        glDeleteShader(evs);
        glDeleteShader(efs);
    } else {
        printf("[BlackHole] Warning: energy stream shaders not found\n");
    }

    // ---- Compile solar wind shader program ----
    std::string wVertSrc = ReadFile("shaders/star_wind_vert.glsl");
    std::string wFragSrc = ReadFile("shaders/star_wind_frag.glsl");
    unsigned int windProgram = 0;
    int wuWindColor = -1, wuWindSize = -1;

    printf("[BlackHole] Wind shaders: vert=%zu bytes, frag=%zu bytes\n", wVertSrc.size(), wFragSrc.size());

    if (!wVertSrc.empty() && !wFragSrc.empty()) {
        const char* wvSrc = wVertSrc.c_str();
        unsigned int wvs = glCreateShader(0x8B31/*GL_VERTEX_SHADER*/);
        glShaderSource(wvs, 1, &wvSrc, nullptr);
        bool wvOk = CompileShader(wvs, "wind_vertex");

        const char* wfSrc = wFragSrc.c_str();
        unsigned int wfs = glCreateShader(0x8B30/*GL_FRAGMENT_SHADER*/);
        glShaderSource(wfs, 1, &wfSrc, nullptr);
        bool wfOk = CompileShader(wfs, "wind_fragment");

        if (wvOk && wfOk) {
            windProgram = CreateProgram(wvs, wfs);
            if (windProgram) {
                wuWindColor = glGetUniformLocation(windProgram, "uWindColor");
                wuWindSize  = glGetUniformLocation(windProgram, "uWindSize");
                printf("[BlackHole] Wind shader program linked OK (color=%d, size=%d)\n",
                       wuWindColor, wuWindSize);
            }
        }
        glDeleteShader(wvs);
        glDeleteShader(wfs);
    } else {
        printf("[BlackHole] Warning: wind shaders not found, solar wind disabled\n");
    }

    // Create solar wind VAO/VBO (reuse particle VAO/VBO pattern)
    unsigned int windVao = 0, windVbo = 0;
    if (windProgram) {
        glGenVertexArrays(1, &windVao);
        glGenBuffers(1, &windVbo);

        glBindVertexArray(windVao);
        glBindBuffer(0x8892/*GL_ARRAY_BUFFER*/, windVbo);
        // Each wind vertex: vec2 pos + float life + float speed = 4 floats
        glBufferData(0x8892/*GL_ARRAY_BUFFER*/,
                     MAX_WIND_PARTICLES * 4 * sizeof(float), nullptr,
                     0x88E8/*GL_DYNAMIC_DRAW*/);

        // location 0: vec2 aPos
        glVertexAttribPointer(0, 2, 0x1406/*GL_FLOAT*/, 0, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // location 1: float aLife
        glVertexAttribPointer(1, 1, 0x1406/*GL_FLOAT*/, 0, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        // location 2: float aSpeed
        glVertexAttribPointer(2, 1, 0x1406/*GL_FLOAT*/, 0, 4 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
        printf("[BlackHole] Wind VAO ready\n");
    }

    // Create particle VAO/VBO (dynamic, updated every frame)
    unsigned int particleVao = 0, particleVbo = 0;
    if (particleProgram) {
        glGenVertexArrays(1, &particleVao);
        glGenBuffers(1, &particleVbo);
        printf("[BlackHole] Particle GL objects created: vao=%u, vbo=%u\n", particleVao, particleVbo);

        glBindVertexArray(particleVao);
        glBindBuffer(0x8892/*GL_ARRAY_BUFFER*/, particleVbo);
        // Pre-allocate for max trail ribbon: each particle has (MAX_TRAIL_LENGTH-1) segments,
        // each segment = 2 triangles = 6 vertices, each vertex = vec2 pos + float alpha = 3 floats
        int maxTrailVertices = MAX_PARTICLES * (MAX_TRAIL_LENGTH - 1) * 6;
        glBufferData(0x8892/*GL_ARRAY_BUFFER*/,
                     maxTrailVertices * 3 * sizeof(float), nullptr,
                     0x88E8/*GL_DYNAMIC_DRAW*/);

        // location 0: vec2 aPos
        glVertexAttribPointer(0, 2, 0x1406/*GL_FLOAT*/, 0, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // location 1: float aLife / aAlpha
        glVertexAttribPointer(1, 1, 0x1406/*GL_FLOAT*/, 0, 3 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        printf("[BlackHole] Particle VAO ready for trail lines\n");
    }

    // Get uniform locations
    int uResolution     = glGetUniformLocation(program, "iResolution");
    int uTime           = glGetUniformLocation(program, "iTime");
    int uDate           = glGetUniformLocation(program, "iDate");
    int uChannel0       = glGetUniformLocation(program, "iChannel0");
    int uChannel1       = glGetUniformLocation(program, "iChannel1");
    printf("[BlackHole] Uniform locations: uChannel0=%d, uChannel1=%d\n", uChannel0, uChannel1);
    int uBornProgress   = glGetUniformLocation(program, "uBornProgress");
    int uHoleRadius     = glGetUniformLocation(program, "uHoleRadius");
    int uDiskGain       = glGetUniformLocation(program, "uDiskGain");
    int uDiskTemp       = glGetUniformLocation(program, "uDiskTemp");
    int uExposure       = glGetUniformLocation(program, "uExposure");
    int uSpeed          = glGetUniformLocation(program, "uSpeed");
    int uStarGain       = glGetUniformLocation(program, "uStarGain");
    int uDiskIncl       = glGetUniformLocation(program, "uDiskIncl");
    int uFixedSize      = glGetUniformLocation(program, "uFixedSize");
    int uFixedLevel     = glGetUniformLocation(program, "uFixedLevel");
    int uDemoMode       = glGetUniformLocation(program, "uDemoMode");
    printf("[BlackHole] Uniform locations: uDemoMode=%d, uHoleRadius=%d, uDiskTemp=%d, uDiskGain=%d, uExposure=%d, uSpeed=%d\n",
           uDemoMode, uHoleRadius, uDiskTemp, uDiskGain, uExposure, uSpeed);
    int uPlayMode       = glGetUniformLocation(program, "uPlayMode");
    int uSlotSec        = glGetUniformLocation(program, "uSlotSec");
    int uPresetCount    = glGetUniformLocation(program, "uPresetCount");
    int uPresetOffset   = glGetUniformLocation(program, "uPresetOffset");
    int uHomeX          = glGetUniformLocation(program, "uHomeX");
    int uHomeY          = glGetUniformLocation(program, "uHomeY");
    int uStarPositions  = glGetUniformLocation(program, "uStarPositions");
    int uStarSizes      = glGetUniformLocation(program, "uStarSizes");
    int uStarLensStr    = glGetUniformLocation(program, "uStarLensStr");

    // Stellar evolution collapse effects (screen-wide)
    int uShakeOffset    = glGetUniformLocation(program, "uShakeOffset");
    int uCollapseVignette = glGetUniformLocation(program, "uCollapseVignette");
    int uCollapseTensionBg = glGetUniformLocation(program, "uCollapseTension");

    // Desktop icon devour (whole-desktop stretch/swirl field)
    int uDevourEnable  = glGetUniformLocation(program, "uDevourEnable");
    int uDevourHoleUV  = glGetUniformLocation(program, "uDevourHoleUV");
    int uDevourStretch = glGetUniformLocation(program, "uDevourStretch");
    int uDevourSwirl   = glGetUniformLocation(program, "uDevourSwirl");

    // Advanced disk parameter uniforms
    int uDiskRoll       = glGetUniformLocation(program, "uDiskRoll");
    int uDiskInner      = glGetUniformLocation(program, "uDiskInner");
    int uDiskOuter      = glGetUniformLocation(program, "uDiskOuter");
    int uDiskOpac       = glGetUniformLocation(program, "uDiskOpac");
    int uDopplerMix     = glGetUniformLocation(program, "uDopplerMix");
    int uDiskBeam       = glGetUniformLocation(program, "uDiskBeam");
    int uDiskContrast   = glGetUniformLocation(program, "uDiskContrast");
    int uDiskWind       = glGetUniformLocation(program, "uDiskWind");
    int uDiskSpeed      = glGetUniformLocation(program, "uDiskSpeed");
    int uLensDepth      = glGetUniformLocation(program, "uLensDepth");
    int uDilationMin    = glGetUniformLocation(program, "uDilationMin");
    int uTimeDilation   = glGetUniformLocation(program, "uTimeDilation");
    int uJetEnabled     = glGetUniformLocation(program, "uJetEnabled");
    int uJetLength      = glGetUniformLocation(program, "uJetLength");
    int uJetWidth       = glGetUniformLocation(program, "uJetWidth");
    int uJetSpeed       = glGetUniformLocation(program, "uJetSpeed");
    int uJetBrightness  = glGetUniformLocation(program, "uJetBrightness");

    // Kerr metric uniforms
    int uSpinParam = glGetUniformLocation(program, "uSpinParam");
    int uKerrMode  = glGetUniformLocation(program, "uKerrMode");
    int uNumSteps  = glGetUniformLocation(program, "uNumSteps");
    int uColorGradient = glGetUniformLocation(program, "uColorGradient");

    // Merger uniforms
    int uMergerPhase       = glGetUniformLocation(program, "uMergerPhase");
    int uMergerProgress    = glGetUniformLocation(program, "uMergerProgress");
    int uMergerFlash       = glGetUniformLocation(program, "uMergerFlash");
    int uMergerRingdownAmp = glGetUniformLocation(program, "uMergerRingdownAmp");
    int uMergerGWFreq      = glGetUniformLocation(program, "uMergerGWFreq");
    int uMergerBHOffset    = glGetUniformLocation(program, "uMergerBHOffset");
    int uMergerCenter      = glGetUniformLocation(program, "uMergerCenter");

    printf("[BlackHole] Kerr uniforms: uSpinParam=%d, uKerrMode=%d, uNumSteps=%d, uColorGradient=%d\n",
           uSpinParam, uKerrMode, uNumSteps, uColorGradient);

    // Create fullscreen quad
    unsigned int vao = 0, vbo = 0;
    CreateQuad(&vao, &vbo);

    // Initialize GL texture for desktop capture
    GLTextureUpload glTex = GLTex_Init(screenW, screenH);
    if (!glTex.active) {
        fprintf(stderr, "[BlackHole] GLTex_Init failed\n");
        GUI_Shutdown();
        Win32GL_Shutdown(&wgl);
        return 1;
    }

    // Initialize WGC desktop capture
    WGCCapture wgc = {};
    bool captureActive = WGC_Init(&wgc, hmon);
    if (!captureActive) {
        fprintf(stderr, "[BlackHole] WGC capture failed, running without desktop texture\n");
    }

    // Show window
    Win32GL_Show(&wgl);
    Win32GL_EnableLayered(&wgl);

    // Enable click-through by default
    Win32GL_SetClickThrough(&wgl, true);

    printf("[BlackHole] Rendering started (%dx%d), capture=%s\n",
           screenW, screenH, captureActive ? "WGC" : "none");
    printf("[BlackHole] Press F1 to toggle control panel, ESC to quit\n");

    // ---- Particle FBO (offscreen render target for gravitational lensing) ----
    static unsigned int particleFBO = 0;
    static unsigned int particleFBOTex = 0;
    static int particleFBOW = 0, particleFBOH = 0;
    static bool particleFBOAllocated = false;

    if (glGenFramebuffers && glBindFramebuffer && glFramebufferTexture2D) {
        // Create FBO texture
        glGenTextures(1, &particleFBOTex);
        glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, particleFBOTex);
        glTexParameteri(0x0DE1, 0x2801/*GL_TEXTURE_MIN_FILTER*/, 0x2601/*GL_LINEAR*/);
        glTexParameteri(0x0DE1, 0x2800/*GL_TEXTURE_MAG_FILTER*/, 0x2601/*GL_LINEAR*/);
        glTexParameteri(0x0DE1, 0x2802/*GL_TEXTURE_WRAP_S*/, 0x812F/*GL_CLAMP_TO_EDGE*/);
        glTexParameteri(0x0DE1, 0x2803/*GL_TEXTURE_WRAP_T*/, 0x812F/*GL_CLAMP_TO_EDGE*/);
        glTexImage2D(0x0DE1, 0, 0x8058/*GL_RGBA8*/, screenW, screenH, 0,
                     0x1908/*GL_RGBA*/, 0x1401/*GL_UNSIGNED_BYTE*/, nullptr);
        glBindTexture(0x0DE1, 0);

        // Create FBO and attach texture
        glGenFramebuffers(1, &particleFBO);
        glBindFramebuffer(0x8D40/*GL_FRAMEBUFFER*/, particleFBO);
        glFramebufferTexture2D(0x8D40/*GL_FRAMEBUFFER*/, 0x8CE0/*GL_COLOR_ATTACHMENT0*/,
                               0x0DE1/*GL_TEXTURE_2D*/, particleFBOTex, 0);
        unsigned int fbStatus = glCheckFramebufferStatus(0x8D40/*GL_FRAMEBUFFER*/);
        if (fbStatus != 0x8CD5/*GL_FRAMEBUFFER_COMPLETE*/) {
            fprintf(stderr, "[BlackHole] Particle FBO incomplete: 0x%X\n", fbStatus);
        }
        glBindFramebuffer(0x8D40/*GL_FRAMEBUFFER*/, 0);

        particleFBOW = screenW;
        particleFBOH = screenH;
        particleFBOAllocated = true;
        printf("[BlackHole] Particle FBO created: %dx%d, fbo=%u, tex=%u\n", screenW, screenH, particleFBO, particleFBOTex);
    } else {
        fprintf(stderr, "[BlackHole] FBO functions not available, particle lensing disabled\n");
    }

    // Timing
    double lastTime = Win32GL_GetTime(&wgl);
    bool wasGuiActive = false;
    bool f1WasDown = false;

    // Initialize star system
    InitStars();

    // Main loop
    while (Win32GL_PollEvents(&wgl)) {
        double currentTime = Win32GL_GetTime(&wgl);
        float dt = (float)(currentTime - lastTime);
        lastTime = currentTime;

        // F1 toggles control panel (edge detection)
        bool f1Down = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (f1Down && !f1WasDown) {
            GUI_SetActive(!GUI_IsActive());
        }
        f1WasDown = f1Down;

        // Toggle click-through based on GUI state
        bool guiActive = GUI_IsActive();
        if (guiActive != wasGuiActive) {
            Win32GL_SetClickThrough(&wgl, !guiActive);
            wasGuiActive = guiActive;
        }

        // Smooth parameter transitions
        Config_SmoothUpdate(&config, dt);

        // Render black hole
        glViewport(0, 0, wgl.width, wgl.height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(0x4000/*GL_COLOR_BUFFER_BIT*/);

        // Capture desktop frame
        if (captureActive) {
            ID3D11Texture2D* frameTex = WGC_GetFrame(&wgc);
            if (frameTex) {
                D3D11_MAPPED_SUBRESOURCE mapped = {};
                if (WGC_CopyToStaging(&wgc, frameTex, &mapped)) {
                    GLTex_Upload(&glTex, mapped.pData, mapped.RowPitch);
                    WGC_UnmapStaging(&wgc);
                }
                frameTex->Release();
            }
        }

        // Compute black hole center (shared by particles, energy stream, star system, and main shader)
        double t = Win32GL_GetTime(&wgl);
        float mainAspect = (float)wgl.width / (float)wgl.height;
        float mainSpeed = config.speed > 0.0f ? config.speed : 1.0f;
        float mainDilation = config.timeDilation > 0.0f ? config.timeDilation : 1.0f;
        float mainShaderT = (float)t * mainSpeed * mainDilation;
        float bhCx, bhCy;
        // bhShadowR：本帧权威阴影半径（aspect 校正 UV），能量流据此把螺旋钉在真实吸积盘上（T1/D4）
        float bhShadowR = 0.0f;
        ComputeBlackHoleCenter(mainShaderT, mainAspect, config.demoMode, config.fixedLevel, config.fixedSize,
                               &bhCx, &bhCy, &bhShadowR);

        // Apply shared shake to black hole center so overlays (energy stream, particles)
        // move together with the shaken background. Shake is in pixels; convert to UV.
        float shakeU = s_shakeX / (float)wgl.width;
        float shakeV = s_shakeY / (float)wgl.height;
        bhCx += shakeU;
        bhCy += shakeV;

        // ---- Overlay 锚点：黑洞中心(UV,Y-up) → 屏幕物理像素 ----
        // 图标吞噬（ListView 客户区）与窗口吞噬（GL 局部像素）共用此锚点；
        // hoist 到循环作用域：中键触发与碎片渲染在帧内稍后引用。
        float holeScreenX = 0.0f, holeScreenY = 0.0f;
        float rHidePx = 40.0f;
        {
            MONITORINFO bhMi = {};
            bhMi.cbSize = sizeof(bhMi);
            if (!GetMonitorInfoW(wgl.hmon, &bhMi)) {
                bhMi.rcMonitor.left = 0;
                bhMi.rcMonitor.top  = 0;
            }
            holeScreenX = (float)bhMi.rcMonitor.left + bhCx * (float)wgl.width;
            holeScreenY = (float)bhMi.rcMonitor.top  + (1.0f - bhCy) * (float)wgl.height;

            // 视界遮蔽半径（像素）：由 holeRadius(UV) 粗略换算，TODO 接入更精确的视界半径。
            float holeUvForHide = config.holeRadius > 0.0f ? config.holeRadius : 0.02f;
            rHidePx = holeUvForHide * (float)wgl.width * 2.0f;
            if (rHidePx < 30.0f) rHidePx = 30.0f;

            DesktopIcons_SetParams(config.iconDevourRate, config.iconDevourSpin,
                                   config.iconDevourStagger, config.iconDevourThrottleHz,
                                   config.iconDevourRestore);
            DesktopIcons_SetRHide(rHidePx);
            DesktopIcons_Update(holeScreenX, holeScreenY, dt);

            WindowDevour_SetParams(config.winDevourRate, config.winDevourSpin,
                                   config.winDevourWarp, config.winDevourStagger);
            WindowDevour_SetRHide(rHidePx);
        }

        // ---- Render particle system to FBO (for gravitational lensing) ----
        if (config.particleEnabled && particleProgram && particleFBOAllocated) {
            glBindFramebuffer(0x8D40/*GL_FRAMEBUFFER*/, particleFBO);
            glViewport(0, 0, particleFBOW, particleFBOH);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(0x4000/*GL_COLOR_BUFFER_BIT*/);

            static bool particleDebugOnce = true;
            if (particleDebugOnce) {
                printf("[BlackHole] Particle rendering enabled (FBO): count=%d\n", config.particleCount);
                particleDebugOnce = false;
            }

            int newTrailLength = config.particleTrailLength;
            if (newTrailLength < 5) newTrailLength = 5;
            if (newTrailLength > MAX_TRAIL_LENGTH) newTrailLength = MAX_TRAIL_LENGTH;
            bool trailLengthChanged = (newTrailLength != s_trailLength);
            s_trailLength = newTrailLength;

            if (!s_particlesInitialized || s_targetParticleCount != config.particleCount || trailLengthChanged) {
                s_targetParticleCount = config.particleCount;
                s_activeParticleCount = s_targetParticleCount;
                if (s_activeParticleCount > MAX_PARTICLES) s_activeParticleCount = MAX_PARTICLES;
                for (int i = 0; i < s_activeParticleCount; i++) {
                    InitParticle(s_particles[i]);
                    float spawnAngle = RandomFloat() * 6.2831853f;
                    float spawnRadius = 0.35f + RandomFloat() * 0.35f;
                    s_particles[i].x = bhCx + cosf(spawnAngle) * spawnRadius;
                    s_particles[i].y = bhCy + sinf(spawnAngle) * spawnRadius;
                    s_particles[i].vx = spawnAngle;
                    s_particles[i].vy = spawnRadius;
                    for (int j = 0; j < MAX_TRAIL_LENGTH; j++) {
                        s_particles[i].trailX[j] = s_particles[i].x;
                        s_particles[i].trailY[j] = s_particles[i].y;
                    }
                    s_particles[i].trailCount = s_trailLength;
                }
                s_particlesInitialized = true;
            }

            UpdateParticles(dt, config.particleGravity, config.particleSpiral, bhCx, bhCy,
                            config.kerrMode ? config.spinParam : 0.0f);

            static float s_trailData[MAX_PARTICLES * (MAX_TRAIL_LENGTH - 1) * 6 * 3];
            static float s_cloudData[MAX_PARTICLES * MAX_TRAIL_LENGTH * 3];
            int vertexCount = 0;
            int cloudCount = 0;
            const float baseWidthUV = 0.0012f;
            // Three-axis tilt control
            float tiltXCos = cosf(config.particleTiltX);  // front-back tilt (compress Y)
            float tiltYCos = cosf(config.particleTiltY);  // left-right tilt (compress X)
            float tiltZCos = cosf(config.particleTiltZ);
            float tiltZSin = sinf(config.particleTiltZ);
            if (tiltXCos < 0.05f) tiltXCos = 0.05f;
            if (tiltYCos < 0.05f) tiltYCos = 0.05f;

            // S5 联动：恒星潮汐剥离的物质最终汇入吸积盘 → 粒子盘亮度随撕裂强度提升。
            // Why: 只调顶点 alpha 增益，绝不改 particleCount（那会触发全量重生、打乱盘面结构）。
            // 注：s_tidalLink 由恒星更新循环写入，这里读到的是上一帧的值（1 帧滞后，约 16ms，不可见）。
            const float particleLinkBoost = 1.0f + 1.5f * s_tidalLink;

            for (int i = 0; i < s_activeParticleCount; i++) {
                Particle& p = s_particles[i];
                int n = p.trailCount;
                if (n < 2) continue;
                for (int j = 0; j < n - 1 && vertexCount + 6 <= MAX_PARTICLES * (MAX_TRAIL_LENGTH - 1) * 6; j++) {
                    float t0 = (float)(j + 0) / (float)(s_trailLength - 1);
                    float t1 = (float)(j + 1) / (float)(s_trailLength - 1);
                    float a0 = (1.0f - t0) * p.life * particleLinkBoost;
                    float a1 = (1.0f - t1) * p.life * particleLinkBoost;
                    if (a0 < 0.005f && a1 < 0.005f) continue;

                    // Apply three-axis tilt: Z rotation -> Y tilt -> X tilt
                    float dx0 = p.trailX[j] - bhCx;
                    float dy0 = p.trailY[j] - bhCy;
                    // 1. Rotate around Z (particleTiltZ)
                    float rx0 = dx0 * tiltZCos - dy0 * tiltZSin;
                    float ry0 = dx0 * tiltZSin + dy0 * tiltZCos;
                    // 2. Compress X (left-right tilt) and Y (front-back tilt)
                    rx0 *= tiltYCos;
                    ry0 *= tiltXCos;
                    // 3. Rotate back around Z
                    float x0 = bhCx + rx0 * tiltZCos + ry0 * tiltZSin;
                    float y0 = bhCy + -rx0 * tiltZSin + ry0 * tiltZCos;

                    float dx1 = p.trailX[j + 1] - bhCx;
                    float dy1 = p.trailY[j + 1] - bhCy;
                    // 1. Rotate around Z (particleTiltZ)
                    float rx1 = dx1 * tiltZCos - dy1 * tiltZSin;
                    float ry1 = dx1 * tiltZSin + dy1 * tiltZCos;
                    // 2. Compress X (left-right tilt) and Y (front-back tilt)
                    rx1 *= tiltYCos;
                    ry1 *= tiltXCos;
                    // 3. Rotate back around Z
                    float x1 = bhCx + rx1 * tiltZCos + ry1 * tiltZSin;
                    float y1 = bhCy + -rx1 * tiltZSin + ry1 * tiltZCos;
                    float ddx = x1 - x0;
                    float ddy = y1 - y0;
                    float segLen = sqrtf(ddx * ddx + ddy * ddy);
                    if (segLen < 1e-6f) continue;
                    float nnx = -ddy / segLen;
                    float nny =  ddx / segLen;
                    float segNoise = sinf((float)i * 12.9898f + (float)j * 78.233f) * 43758.5453f;
                    segNoise = segNoise - floorf(segNoise);
                    float segWidth = 0.4f + segNoise * 1.4f;
                    float w0 = config.particleSize * baseWidthUV * p.streamWidth * segWidth;
                    float x0p = x0 + nnx * w0, y0p = y0 + nny * w0;
                    float x0m = x0 - nnx * w0, y0m = y0 - nny * w0;
                    float x1p = x1 + nnx * w0, y1p = y1 + nny * w0;
                    float x1m = x1 - nnx * w0, y1m = y1 - nny * w0;
#define PUSH_VERT(buf, idx, xv, yv, av) do { buf[(idx)*3+0]=(xv); buf[(idx)*3+1]=(yv); buf[(idx)*3+2]=(av); (idx)++; } while(0)
                    PUSH_VERT(s_trailData, vertexCount, x0m, y0m, a0);
                    PUSH_VERT(s_trailData, vertexCount, x0p, y0p, a0);
                    PUSH_VERT(s_trailData, vertexCount, x1p, y1p, a1);
                    PUSH_VERT(s_trailData, vertexCount, x0m, y0m, a0);
                    PUSH_VERT(s_trailData, vertexCount, x1p, y1p, a1);
                    PUSH_VERT(s_trailData, vertexCount, x1m, y1m, a1);
#undef PUSH_VERT
                }
                if (config.particleCloudEnabled && cloudCount + n * 2 <= MAX_PARTICLES * MAX_TRAIL_LENGTH) {
                    for (int j = 0; j < n; j += 2) {
                        // Apply three-axis tilt to cloud particles
                        float dx = p.trailX[j] - bhCx;
                        float dy = p.trailY[j] - bhCy;
                        float rx = dx * tiltZCos - dy * tiltZSin;
                        float ry = dx * tiltZSin + dy * tiltZCos;
                        rx *= tiltYCos;
                        ry *= tiltXCos;
                        float cxp = bhCx + rx * tiltZCos + ry * tiltZSin;
                        float cyp = bhCy + -rx * tiltZSin + ry * tiltZCos;
                        float r = sqrtf((cxp - bhCx) * (cxp - bhCx) + (cyp - bhCy) * (cyp - bhCy));
                        float cloudAlpha = 0.15f * (1.0f - (float)j / (float)(s_trailLength - 1))
                                         * p.life * particleLinkBoost;
                        if (cloudAlpha < 0.01f) continue;
                        float angle = RandomFloat() * 6.2831853f;
                        float dist = (0.02f + r * 0.15f) * (0.5f + RandomFloat());
                        float ox = cosf(angle) * dist;
                        float oy = sinf(angle) * dist * tiltXCos;  // use Y tilt for vertical compression
                        s_cloudData[cloudCount * 3 + 0] = cxp + ox;
                        s_cloudData[cloudCount * 3 + 1] = cyp + oy;
                        s_cloudData[cloudCount * 3 + 2] = cloudAlpha * (0.7f + RandomFloat() * 0.5f);
                        cloudCount++;
                    }
                }
            }

            glEnable(0x0BE2/*GL_BLEND*/);
            glBlendFunc(0x0302/*GL_SRC_ALPHA*/, 0x0001/*GL_ONE*/);
            glDisable(0x0B71/*GL_DEPTH_TEST*/);
            glUseProgram(particleProgram);
            if (puParticleColor >= 0) glUniform3f(puParticleColor, config.particleColorR, config.particleColorG, config.particleColorB);
            if (puColorGradientParticle >= 0) glUniform1f(puColorGradientParticle, config.colorGradient);
            if (puBHCenter >= 0) glUniform2f(puBHCenter, bhCx, bhCy);
            if (puPointSize >= 0) glUniform1f(puPointSize, config.particleSize);

            glBindBuffer(0x8892/*GL_ARRAY_BUFFER*/, particleVbo);
            glBufferSubData(0x8892/*GL_ARRAY_BUFFER*/, 0, vertexCount * 3 * sizeof(float), s_trailData);
            glBindVertexArray(particleVao);
            glDrawArrays(0x0004/*GL_TRIANGLES*/, 0, vertexCount);

            if (config.particleCloudEnabled && cloudCount > 0) {
                if (puPointSize >= 0) glUniform1f(puPointSize, config.particleSize * 0.7f);
                glBufferSubData(0x8892/*GL_ARRAY_BUFFER*/, 0, cloudCount * 3 * sizeof(float), s_cloudData);
                glDrawArrays(0x0000/*GL_POINTS*/, 0, cloudCount);
            }

            glBindVertexArray(0);
            glUseProgram(0);

            glBindFramebuffer(0x8D40/*GL_FRAMEBUFFER*/, 0);
            glViewport(0, 0, wgl.width, wgl.height);
            glDisable(0x0BE2/*GL_BLEND*/);
            glBlendFunc(0x0302/*GL_SRC_ALPHA*/, 0x0303/*GL_ONE_MINUS_SRC_ALPHA*/);
        } else if (!config.particleEnabled) {
            s_particlesInitialized = false;
            s_targetParticleCount = 0;
            if (particleFBOAllocated) {
                glBindFramebuffer(0x8D40/*GL_FRAMEBUFFER*/, particleFBO);
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(0x4000/*GL_COLOR_BUFFER_BIT*/);
                glBindFramebuffer(0x8D40/*GL_FRAMEBUFFER*/, 0);
            }
        }

        // ---- Render main black hole shader ----
        glUseProgram(program);

        // Time uniforms
        if (uTime >= 0) glUniform1f(uTime, (float)t);
        if (uResolution >= 0) glUniform3f(uResolution, (float)wgl.width, (float)wgl.height, 1.0f);

        // Date uniform
        time_t now = time(nullptr);
        struct tm* lt = localtime(&now);
        if (uDate >= 0) glUniform4f(uDate, (float)(lt->tm_year + 1900), (float)(lt->tm_mon + 1), (float)lt->tm_mday, (float)(lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec));

        // Desktop texture
        if (uChannel0 >= 0) {
            glActiveTexture(0x84C0/*GL_TEXTURE0*/);
            glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, GLTex_GetTexture(&glTex));
            glUniform1i(uChannel0, 0);
        }

        // Particle FBO texture (iChannel1)
        if (uChannel1 >= 0 && particleFBOAllocated) {
            glActiveTexture(0x84C1/*GL_TEXTURE1*/);
            glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, particleFBOTex);
            glUniform1i(uChannel1, 1);
            glActiveTexture(0x84C0/*GL_TEXTURE0*/);
        }

        // Born progress
        if (uBornProgress >= 0) glUniform1f(uBornProgress, 1.0f);

        // Supermassive black hole mode overrides
        float effectiveDiskInner  = config.diskInner;
        float effectiveDiskOuter  = config.diskOuter;
        float effectiveDiskGain   = config.diskGain;
        float effectiveDiskTemp   = config.diskTemp;
        float effectiveExposure   = config.exposure;
        float effectiveDiskOpac   = config.diskOpac;
        float effectiveDiskContrast = config.diskContrast;
        float effectiveDiskBeam   = config.diskBeam;
        float effectiveHoleRadius = config.holeRadius;
        // Accretion disk inclination always follows the user/global setting
        float effectiveDiskIncl = config.diskIncl;

        if (config.supermassiveMode) {
            // Use shader defaults where user hasn't overridden, but scale them up
            if (effectiveHoleRadius < 0.0f) effectiveHoleRadius = 0.02f;
            if (effectiveDiskInner  < 0.0f) effectiveDiskInner  = 1.8f;
            if (effectiveDiskOuter  < 0.0f) effectiveDiskOuter  = 8.0f;
            if (effectiveDiskGain   < 0.0f) effectiveDiskGain   = 2.2f;
            if (effectiveDiskTemp   < 0.0f) effectiveDiskTemp   = 5500.0f;
            if (effectiveExposure   < 0.0f) effectiveExposure   = 1.4f;
            if (effectiveDiskOpac   < 0.0f) effectiveDiskOpac   = 0.9f;
            if (effectiveDiskContrast < 0.0f) effectiveDiskContrast = 1.6f;
            if (effectiveDiskBeam   < 0.0f) effectiveDiskBeam   = 2.5f;

            // Scale up the accretion disk and brighten it
            effectiveHoleRadius   *= 1.5f;
            effectiveDiskInner    *= 0.8f;
            effectiveDiskOuter    *= 2.2f;
            effectiveDiskGain     *= 2.5f;
            effectiveDiskTemp      = fmaxf(effectiveDiskTemp * 0.85f, 3000.0f); // redder
            effectiveExposure     *= 1.5f;
            effectiveDiskOpac      = 1.0f;
            effectiveDiskContrast *= 1.5f;
            effectiveDiskBeam     *= 1.5f;
        }

        // S5 联动：恒星剥离的物质落到吸积盘上 → 盘面整体增亮（乘性增益，不改几何）。
        // 同样存在 1 帧滞后（s_tidalLink 由恒星更新循环写入）。
        if (config.tidalLinkAccretion > 0.0f) {
            effectiveDiskGain *= (1.0f + 0.8f * s_tidalLink * config.tidalLinkAccretion);
        }

        // Shader parameter overrides from config
        // -1.0 = use shader default (uniform <= 0 triggers default in patched shader)
        if (uHoleRadius >= 0) glUniform1f(uHoleRadius, effectiveHoleRadius);
        if (uDiskGain >= 0)   glUniform1f(uDiskGain,   effectiveDiskGain);
        if (uDiskTemp >= 0)   glUniform1f(uDiskTemp,   effectiveDiskTemp);
        if (uExposure >= 0)   glUniform1f(uExposure,   effectiveExposure);
        if (uSpeed >= 0)      glUniform1f(uSpeed,      config.speed);
        if (uStarGain >= 0)   glUniform1f(uStarGain,   config.starGain);
        if (uDiskIncl >= 0)   glUniform1f(uDiskIncl,   effectiveDiskIncl);

        // Advanced disk parameter overrides
        if (uDiskRoll >= 0)     glUniform1f(uDiskRoll,     config.diskRoll);
        if (uDiskInner >= 0)    glUniform1f(uDiskInner,    effectiveDiskInner);
        if (uDiskOuter >= 0)    glUniform1f(uDiskOuter,    effectiveDiskOuter);
        if (uDiskOpac >= 0)     glUniform1f(uDiskOpac,     effectiveDiskOpac);
        if (uDopplerMix >= 0)   glUniform1f(uDopplerMix,   config.dopplerMix);
        if (uDiskBeam >= 0)     glUniform1f(uDiskBeam,     effectiveDiskBeam);
        if (uDiskContrast >= 0) glUniform1f(uDiskContrast, effectiveDiskContrast);
        if (uDiskWind >= 0)     glUniform1f(uDiskWind,     config.diskWind);
        if (uDiskSpeed >= 0)    glUniform1f(uDiskSpeed,    config.diskSpeed);
        if (uLensDepth >= 0)    glUniform1f(uLensDepth,    config.lensDepth);
        if (uDilationMin >= 0)  glUniform1f(uDilationMin,  config.dilationMin);
        if (uTimeDilation >= 0) glUniform1f(uTimeDilation, config.timeDilation);

        // Jet uniforms
        // S5 联动：撕裂事件本身会驱动极区喷流（TDE 的经典观测特征）。
        // 联动量 L > 0.25 时自动开启喷流，且亮度随 L 放大；用户手动开启时保持原行为。
        float jetLinkBoost = 1.0f + 2.0f * s_tidalLink * config.tidalLinkAccretion;
        bool  jetAutoOn    = config.tidalLinkAccretion > 0.0f && s_tidalLink > 0.25f;
        if (uJetEnabled >= 0)    glUniform1f(uJetEnabled,    (config.jetEnabled || jetAutoOn) ? 1.0f : 0.0f);
        if (uJetLength >= 0)     glUniform1f(uJetLength,     config.jetLength);
        if (uJetWidth >= 0)      glUniform1f(uJetWidth,      config.jetWidth);
        if (uJetSpeed >= 0)      glUniform1f(uJetSpeed,      config.jetSpeed);
        if (uJetBrightness >= 0) glUniform1f(uJetBrightness, config.jetBrightness * jetLinkBoost);

        // Kerr metric uniforms
        if (uSpinParam >= 0) glUniform1f(uSpinParam, config.kerrMode ? config.spinParam : 0.0f);
        if (uKerrMode >= 0)  glUniform1f(uKerrMode, config.kerrMode ? 1.0f : 0.0f);
        if (uNumSteps >= 0)  glUniform1i(uNumSteps, config.kerrMode ? config.numSteps : 48);
        if (uColorGradient >= 0) glUniform1f(uColorGradient, config.colorGradient);

        // Merger uniforms: find active merger star
        float mergerPhase = 0.0f, mergerProgress = 0.0f, mergerFlash = 0.0f;
        float mergerRingdownAmp = 0.0f, mergerGWFreq = 0.0f;
        float mergerBHOX = 0.0f, mergerBHOY = 0.0f;
        float mergerCenterX = 0.5f, mergerCenterY = 0.5f;
        for (int i = 0; i < s_starCount; i++) {
            StarObj& ms = s_stars[i];
            if (ms.state == STAR_DEAD) continue;
            if (ms.mergerPhase != MERGER_IDLE && ms.mergerPhase != MERGER_DONE) {
                mergerPhase = (float)ms.mergerPhase;
                if (ms.mergerPhase == MERGER_INSPIRAL)
                    mergerProgress = ms.mergerTimer / config.mergerInspiralDur;
                else if (ms.mergerPhase == MERGER_MERGER)
                    mergerProgress = ms.mergerTimer / config.mergerMergerDur;
                else if (ms.mergerPhase == MERGER_RINGDOWN)
                    mergerProgress = ms.mergerTimer / config.mergerRingdownDur;
                mergerFlash = ms.mergerFlash;
                mergerRingdownAmp = ms.mergerRingdownAmp;
                mergerGWFreq = ms.mergerGWFreq;
                mergerBHOX = ms.mergerMainBHOffsetX;
                mergerBHOY = ms.mergerMainBHOffsetY;
                mergerCenterX = ms.mergerMainBHStartX;
                mergerCenterY = ms.mergerMainBHStartY;
                break;
            }
        }
        if (uMergerPhase >= 0)       glUniform1f(uMergerPhase, mergerPhase);
        if (uMergerProgress >= 0)    glUniform1f(uMergerProgress, mergerProgress);
        if (uMergerFlash >= 0)       glUniform1f(uMergerFlash, mergerFlash * config.mergerFlashBright);
        if (uMergerRingdownAmp >= 0) glUniform1f(uMergerRingdownAmp, mergerRingdownAmp);
        if (uMergerGWFreq >= 0)      glUniform1f(uMergerGWFreq, mergerGWFreq);
        if (uMergerBHOffset >= 0)    glUniform2f(uMergerBHOffset, mergerBHOX, mergerBHOY);
        if (uMergerCenter >= 0)      glUniform2f(uMergerCenter, mergerCenterX, mergerCenterY);

        // Stellar evolution collapse effects (screen-wide, from previous frame's star update)
        if (uShakeOffset >= 0)      glUniform2f(uShakeOffset, s_shakeX, s_shakeY);
        if (uCollapseVignette >= 0) glUniform1f(uCollapseVignette, s_evolveVignette);
        if (uCollapseTensionBg >= 0) glUniform1f(uCollapseTensionBg, s_evolveTension);

        // Desktop icon devour field uniforms (black hole center in UV space)
        {
            bool devourOn = config.iconDevourEnabled && DesktopIcons_IsDevouring();
            if (uDevourEnable >= 0)  glUniform1i(uDevourEnable, devourOn ? 1 : 0);
            if (uDevourHoleUV >= 0)  glUniform2f(uDevourHoleUV, bhCx, bhCy);
            if (uDevourStretch >= 0) glUniform1f(uDevourStretch, config.iconDevourStretch);
            if (uDevourSwirl >= 0)   glUniform1f(uDevourSwirl, config.iconDevourSwirl);
        }

        // Playback control from config
        if (uFixedSize >= 0)    glUniform1i(uFixedSize, config.fixedSize ? 1 : 0);
        if (uFixedLevel >= 0)   glUniform1f(uFixedLevel, config.fixedLevel);
        if (uDemoMode >= 0)     glUniform1f(uDemoMode, config.demoMode ? 1.0f : 0.0f);
        // Debug: log first frame
        static bool firstFrame = true;
        if (firstFrame) {
            printf("[BlackHole] First frame: demoMode=%d holeRadius=%.2f diskTemp=%.0f diskGain=%.2f exposure=%.2f\n",
                   (int)config.demoMode, config.holeRadius, config.diskTemp, config.diskGain, config.exposure);
            firstFrame = false;
        }
        if (uPlayMode >= 0)     glUniform1i(uPlayMode, config.playMode);
        if (uSlotSec >= 0)      glUniform1f(uSlotSec, config.slotSec);
        if (uPresetCount >= 0)  glUniform1i(uPresetCount, config.presetCount);
        if (uPresetOffset >= 0) glUniform1f(uPresetOffset, 0.0f);
        if (uHomeX >= 0)        glUniform1f(uHomeX, 0.96f);
        if (uHomeY >= 0)        glUniform1f(uHomeY, 0.04f);

        // Star gravitational lensing uniforms
        {
            int activeStarCount = 0;
            float star0X = 0.0f, star0Y = 0.0f, star0Size = 0.0f;
            float star1Size = 0.0f, star2Size = 0.0f;
            for (int i = 0; i < s_starCount; i++) {
                if (s_stars[i].state != STAR_DEAD) {
                    if (activeStarCount == 0) {
                        star0X = s_stars[i].x;
                        star0Y = s_stars[i].y;
                        star0Size = s_stars[i].size;
                    } else if (activeStarCount == 1) {
                        star1Size = s_stars[i].size;
                    } else if (activeStarCount == 2) {
                        star2Size = s_stars[i].size;
                    }
                    activeStarCount++;
                }
            }
            if (uStarPositions >= 0) glUniform3f(uStarPositions, star0X, star0Y, (float)activeStarCount);
            if (uStarSizes >= 0)     glUniform3f(uStarSizes, star0Size, star1Size, star2Size);
            float lensStr = config.starLensStrength * (activeStarCount > 0 ? 1.0f : 0.0f);
            if (uStarLensStr >= 0)   glUniform1f(uStarLensStr, lensStr);
        }

        // Draw fullscreen quad
        glBindVertexArray(vao);
        glDrawArrays(0x0005/*GL_TRIANGLE_STRIP*/, 0, 4);
        glBindVertexArray(0);

        glUseProgram(0);

        // ---- Capture framebuffer for mini BH gravitational lensing ----
        static unsigned int fbCaptureTex = 0;
        static int fbCaptureW = 0, fbCaptureH = 0;
        static bool fbCaptureAllocated = false;
        if (config.starEnabled && starProgram && glCopyTexImage2D && glCopyTexSubImage2D) {
            if (!fbCaptureAllocated || fbCaptureW != wgl.width || fbCaptureH != wgl.height) {
                if (fbCaptureTex) glDeleteTextures(1, &fbCaptureTex);
                glGenTextures(1, &fbCaptureTex);
                glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, fbCaptureTex);
                glTexParameteri(0x0DE1, 0x2801/*GL_TEXTURE_MIN_FILTER*/, 0x2601/*GL_LINEAR*/);
                glTexParameteri(0x0DE1, 0x2800/*GL_TEXTURE_MAG_FILTER*/, 0x2601/*GL_LINEAR*/);
                glTexParameteri(0x0DE1, 0x2802/*GL_TEXTURE_WRAP_S*/, 0x812F/*GL_CLAMP_TO_EDGE*/);
                glTexParameteri(0x0DE1, 0x2803/*GL_TEXTURE_WRAP_T*/, 0x812F/*GL_CLAMP_TO_EDGE*/);
                glCopyTexImage2D(0x0DE1/*GL_TEXTURE_2D*/, 0, 0x8058/*GL_RGBA8*/,
                                 0, 0, wgl.width, wgl.height, 0);
                fbCaptureW = wgl.width;
                fbCaptureH = wgl.height;
                fbCaptureAllocated = true;
            } else {
                glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, fbCaptureTex);
                glCopyTexSubImage2D(0x0DE1/*GL_TEXTURE_2D*/, 0, 0, 0, 0, 0,
                                    wgl.width, wgl.height);
            }
            glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, 0);
        }

        // ---- Render star system ----
        if (config.starEnabled && starProgram) {
            // Mouse interaction: check if left mouse button is held
            bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            static bool mouseWasDown = false;
            static float mouseX = 0.5f, mouseY = 0.5f;

            // Get mouse position in UV coordinates
            POINT cursorPos;
            if (GetCursorPos(&cursorPos)) {
                // Convert screen coords to window UV
                // Since this is a fullscreen window, screen coords = window coords
                mouseX = (float)cursorPos.x / (float)wgl.width;
                mouseY = 1.0f - (float)cursorPos.y / (float)wgl.height; // flip Y
                if (mouseX < 0.0f) mouseX = 0.0f; if (mouseX > 1.0f) mouseX = 1.0f;
                if (mouseY < 0.0f) mouseY = 0.0f; if (mouseY > 1.0f) mouseY = 1.0f;
            }

            // Mouse click: create new star
            if (mouseDown && !mouseWasDown && s_starCount < MAX_STARS) {
                // Find a dead slot or the oldest fading star
                int slot = -1;
                for (int i = 0; i < MAX_STARS; i++) {
                    if (s_stars[i].state == STAR_DEAD) { slot = i; break; }
                }
                if (slot < 0) {
                    // All slots used, find the one that's fading (closest to dead)
                    float minAlpha = 2.0f;
                    for (int i = 0; i < MAX_STARS; i++) {
                        if (s_stars[i].alpha < minAlpha) { minAlpha = s_stars[i].alpha; slot = i; }
                    }
                }
                if (slot >= 0) {
                    StarObj& s = s_stars[slot];
                    s.x = mouseX;
                    s.y = mouseY;
                    s.size = 0.005f; // start small
                    s.alpha = 1.0f;
                    s.age = 0.0f;
                    s.state = STAR_GROWING;
                    s.type = RandomStarType();
                    s.colorPhase = RandomFloat() * 6.28f;
                    GetStarTypeColor(s.type, &s.baseR, &s.baseG, &s.baseB);
                    // Evolution init
                    s.stage = STAGE_NORMAL;
                    s.evo = EVO_IDLE;
                    s.evoTimer = 0.0f;
                    s.collapseProgress = -1.0f;
                    s.supernovaProgress = -1.0f;
                    s.expandProgress = -1.0f;
                    s.nsRotation = 0.0f;
                    s.magAxisTilt = RandomFloat() * 1.57f; // random magnetic axis tilt
                    s.bhFormationProgress = 0.0f;
                    s.vx = 0.0f;
                    s.vy = 0.0f;
                    s.tidalProgress = 0.0f;
                    s.tidalAmp      = 0.0f;
                    s.tidalStrip    = 0.0f;
                    // Merger fields init
                    s.mergerPhase = MERGER_IDLE;
                    s.mergerTimer = 0.0f;
                    s.mergerOrbitAngle = 0.0f;
                    s.mergerFlash = 0.0f;
                    s.mergerRingdownAmp = 0.0f;
                    s.mergerGWFreq = 0.0f;
                    s.mergerBaseSize = 0.0f;
                    s.mergerMainBHOffsetX = 0.0f;
                    s.mergerMainBHOffsetY = 0.0f;
                    s.mergerMainBHStartX = 0.5f;
                    s.mergerMainBHStartY = 0.5f;
                    if (slot >= s_starCount) s_starCount = slot + 1;
                    printf("[BlackHole] Star created: type=%d pos=(%.2f,%.2f) color=(%.2f,%.2f,%.2f)\n",
                           s.type, s.x, s.y, s.baseR, s.baseG, s.baseB);
                }
            }

            // Right-click: trigger stellar evolution (star -> WD -> NS)
            bool rMouseDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            static bool rMouseWasDown = false;
            if (rMouseDown && !rMouseWasDown && config.starEvolutionEnabled) {
                // Find the first star that can evolve
                for (int i = 0; i < s_starCount; i++) {
                    StarObj& s = s_stars[i];
                    if (s.state == STAR_DEAD) continue;
                    if (s.evo != EVO_IDLE) continue; // already evolving

                    if (s.stage == STAGE_NORMAL && (s.state == STAR_ALIVE || s.state == STAR_GROWING)) {
                        // Right-click #1: expand to max, then collapse to white dwarf
                        s.evo = EVO_EXPANDING;
                        s.evoTimer = 0.0f;
                        s.expandProgress = 0.0f;
                        s.collapseProgress = -1.0f;
                        s.state = STAR_ALIVE; // pin alive during evolution
                        printf("[BlackHole] Star %d: evolution stage 1 -> expanding -> white dwarf\n", i);
                        break;
                    } else if (s.stage == STAGE_WHITE_DWARF) {
                        // Right-click #2: collapse to dense sphere -> supernova -> neutron star
                        s.evo = EVO_COLLAPSING;
                        s.evoTimer = 0.0f;
                        s.collapseProgress = 0.0f;
                        s.supernovaProgress = -1.0f;
                        printf("[BlackHole] Star %d: evolution stage 2 -> collapsing -> supernova -> neutron star\n", i);
                        break;
                    }
                }
            }
            rMouseWasDown = rMouseDown;

            // Middle-click handling moved outside the star gate (see two-key coexistence
            // block near the end of the main loop).

            // M key: trigger black hole merger
            static bool mKeyWasDown = false;
            bool mKeyDown = (GetAsyncKeyState('M') & 0x8000) != 0;
            if (mKeyDown && !mKeyWasDown && config.mergerEnabled) {
                for (int i = 0; i < s_starCount; i++) {
                    StarObj& s = s_stars[i];
                    if (s.state == STAR_DEAD) continue;
                    if (s.stage == STAGE_BLACK_HOLE && s.evo == EVO_IDLE && s.mergerPhase == MERGER_IDLE) {
                        // Calculate direction: mini BH should start from far side of main BH
                        float dx = s.x - bhCx;
                        float dy = s.y - bhCy;
                        float dist = sqrtf(dx*dx + dy*dy);
                        
                        // Set initial position to far side (opposite direction, at initial radius)
                        float startRadius = config.mergerInitialRadius * 1.2f;  // Start a bit further
                        float startAngle = atan2f(dy, dx) + 3.14159f;  // Opposite direction
                        
                        s.x = bhCx + startRadius * cosf(startAngle);
                        s.y = bhCy + startRadius * sinf(startAngle);
                        
                        // Record main BH start position for stable center during merger
                        s.mergerMainBHStartX = bhCx;
                        s.mergerMainBHStartY = bhCy;
                        
                        s.mergerPhase = MERGER_INSPIRAL;
                        s.mergerTimer = 0.0f;
                        s.mergerOrbitAngle = startAngle;
                        printf("[BlackHole] Star %d: merger triggered (M key), start radius=%.3f\n", i, startRadius);
                        break;
                    }
                }
            }
            mKeyWasDown = mKeyDown;

            // Update stars
            // 帧级联动量在此清零：循环内取所有恒星的 max(strip)，供吸积盘/喷流联动使用
            s_tidalLink = 0.0f;
            for (int i = 0; i < s_starCount; i++) {
                StarObj& s = s_stars[i];
                if (s.state == STAR_DEAD) continue;

                s.age += dt;

                // ===== 潮汐撕裂积分（S1：本体与能量流共用的唯一物理量）=====
                // 物理：牛顿潮汐加速度差 Δa ∝ 2GMR*/r³ ⇒ 形变率 ∝ (r_t/r)³。
                //   β      = r_t / r                         （穿透因子，越大越深入）
                //   T      = clamp(β, 1, β_max)              （有界潮汐强度，禁止无上界发散）
                //   q      = (T - 1) / (β_max - 1) ∈ [0,1]   （归一化强度）
                //   dp/dt  = (0.15 + 0.85·q^0.6) / t_dis     （时间量纲锚点：t_dis 秒完成 0→1）
                // Why: v1.x 用 T=β³ 无上界，β 很大时 τ→0，全流程仅约 15 帧 → "瞬间被拉长"。
                //      有界化后 r=r_t 处 t_full = t_dis，最深入态 t_full = t_dis/1.0。
                {
                    // aspect 校正：shader 在 (x·aspect, y) 空间判定距离，
                    // CPU 必须使用同一空间的度量，否则撕裂区在屏幕上是椭圆（教训 C4）。
                    float tdx = (bhCx - s.x) * mainAspect;
                    float tdy = bhCy - s.y;
                    float tr  = sqrtf(tdx * tdx + tdy * tdy) + 1e-4f;
                    if (config.tidalEnabled && s.stage == STAGE_NORMAL) {
                        float beta    = config.tidalRadius / tr;
                        float betaMax = fmaxf(config.tidalBetaMax, 1.0001f);
                        if (beta > 1.0f) {
                            float T    = fminf(beta, betaMax);
                            float q    = (T - 1.0f) / (betaMax - 1.0f);
                            float rate = (0.15f + 0.85f * powf(q, 0.6f))
                                       / fmaxf(config.tidalDisruptSec, 0.1f);
                            s.tidalProgress += rate * dt;
                        }
                        // 决策：不可逆——被撕开即保持（重新生成恒星只需再点一次左键）
                        s.tidalProgress = fmaxf(0.0f, fminf(1.0f, s.tidalProgress));
                        // 双量分离：本体形变有界且只占一小部分；流强度由 strip 主导（尾部爆发）
                        s.tidalAmp   = config.tidalStretch * powf(s.tidalProgress, 0.8f);
                        s.tidalStrip = powf(s.tidalProgress, 1.2f);
                    } else {
                        s.tidalProgress = 0.0f;
                        s.tidalAmp      = 0.0f;
                        s.tidalStrip    = 0.0f;
                    }
                    if (s.tidalStrip > s_tidalLink) s_tidalLink = s.tidalStrip;
                }

                // Neutron star rotates continuously
                if (s.stage == STAGE_NEUTRON_STAR) {
                    s.nsRotation += dt * config.nsRotationSpeed;
                }

                // ===== Evolution update (takes precedence over normal lifecycle) =====
                if (s.evo != EVO_IDLE) {
                    s.evoTimer += dt;
                    float maxSz = config.starMaxSize;

                    if (s.stage == STAGE_NORMAL) {
                        // Stage 1: expand to max, then collapse to white dwarf
                        float expandTarget = maxSz * 1.4f;
                        float wdSize = maxSz * 0.18f;
                        if (s.evo == EVO_EXPANDING) {
                            s.expandProgress = fminf(1.0f, s.expandProgress + config.evoExpandSpeed * dt);
                            s.size = maxSz + (expandTarget - maxSz) * s.expandProgress;
                            if (s.expandProgress >= 1.0f) {
                                s.evo = EVO_COLLAPSING;
                                s.evoTimer = 0.0f;
                                s.collapseProgress = 0.0f;
                            }
                        } else if (s.evo == EVO_COLLAPSING) {
                            s.collapseProgress = fminf(1.0f, s.collapseProgress + config.evoCollapseSpeed * dt);
                            float startSz = expandTarget;
                            s.size = startSz + (wdSize - startSz) * s.collapseProgress;
                            if (s.collapseProgress >= 1.0f) {
                                // Become white dwarf
                                s.stage = STAGE_WHITE_DWARF;
                                s.evo = EVO_IDLE;
                                s.size = wdSize;
                                s.alpha = 1.0f;
                                s.collapseProgress = -1.0f;
                                s.expandProgress = -1.0f;
                                s.baseR = 0.85f; s.baseG = 0.9f; s.baseB = 1.0f; // white-blue
                                printf("[BlackHole] Star %d became a white dwarf (size=%.3f)\n", i, s.size);
                            }
                        }
                    } else if (s.stage == STAGE_WHITE_DWARF) {
                        // Stage 2: collapse to dense sphere -> supernova -> neutron star
                        float denseSize = maxSz * 0.08f;
                        float nsSize = maxSz * 0.12f;
                        if (s.evo == EVO_COLLAPSING) {
                            s.collapseProgress = fminf(1.0f, s.collapseProgress + config.evoCollapseSpeed * dt);
                            float startSz = maxSz * 0.18f;
                            s.size = startSz + (denseSize - startSz) * s.collapseProgress;
                            if (s.collapseProgress >= 1.0f) {
                                s.evo = EVO_PRE_SUPERNOVA;
                                s.evoTimer = 0.0f;
                                s.size = denseSize;
                            }
                        } else if (s.evo == EVO_PRE_SUPERNOVA) {
                            // Brief tension buildup (~0.4s) before explosion
                            if (s.evoTimer > 0.4f) {
                                s.evo = EVO_SUPERNOVA;
                                s.evoTimer = 0.0f;
                                s.supernovaProgress = 0.0f;
                                printf("[BlackHole] Star %d: SUPERNOVA!\n", i);
                            }
                        } else if (s.evo == EVO_SUPERNOVA) {
                            s.supernovaProgress = fminf(1.0f, s.supernovaProgress + dt / config.supernovaDuration);
                            // Halfway through the explosion, transition to neutron star
                            if (s.supernovaProgress > 0.5f && s.stage != STAGE_NEUTRON_STAR) {
                                s.stage = STAGE_NEUTRON_STAR;
                                s.size = nsSize;
                                s.baseR = 0.6f; s.baseG = 0.5f; s.baseB = 1.0f; // violet-blue
                            }
                            if (s.supernovaProgress >= 1.0f) {
                                s.evo = EVO_SETTLE;
                                s.evoTimer = 0.0f;
                                s.supernovaProgress = -1.0f;
                                s.collapseProgress = -1.0f;
                                s.size = nsSize;
                            }
                        } else if (s.evo == EVO_SETTLE) {
                            if (s.evoTimer > 0.6f) {
                                s.evo = EVO_IDLE;
                                printf("[BlackHole] Star %d settled as neutron star\n", i);
                            }
                        }
                    } else if (s.stage == STAGE_NEUTRON_STAR) {
                        // Stage 3: collapse beyond Oppenheimer limit -> mini black hole
                        float bhSize = maxSz * 0.22f;
                        if (s.evo == EVO_SUPERNOVA) {
                            // Continue supernova progression (stage changed to NS at midpoint)
                            s.supernovaProgress = fminf(1.0f, s.supernovaProgress + dt / config.supernovaDuration);
                            if (s.supernovaProgress >= 1.0f) {
                                s.evo = EVO_SETTLE;
                                s.evoTimer = 0.0f;
                                s.supernovaProgress = -1.0f;
                                s.collapseProgress = -1.0f;
                            }
                        } else if (s.evo == EVO_COLLAPSING) {
                            s.collapseProgress = fminf(1.0f, s.collapseProgress + config.evoCollapseSpeed * dt);
                            float startSz = maxSz * 0.12f;
                            s.size = startSz + (bhSize - startSz) * s.collapseProgress;
                            if (s.collapseProgress >= 1.0f) {
                                s.stage = STAGE_BLACK_HOLE;
                                s.evo = EVO_SETTLE;
                                s.evoTimer = 0.0f;
                                s.collapseProgress = -1.0f;
                                s.size = bhSize;
                                s.bhFormationProgress = 0.0f;
                                printf("[BlackHole] Star %d became a mini black hole\n", i);
                            }
                        } else if (s.evo == EVO_SETTLE) {
                            // Post-supernova settle: brief wait then stable
                            if (s.evoTimer > 0.6f) {
                                s.evo = EVO_IDLE;
                                printf("[BlackHole] Star %d settled as neutron star\n", i);
                            }
                        }
                    } else if (s.stage == STAGE_BLACK_HOLE) {
                        // Mini black hole formation: grow photon ring then stable
                        if (s.evo == EVO_SETTLE) {
                            s.bhFormationProgress = fminf(1.0f, s.bhFormationProgress + dt * 0.8f);
                            if (s.evoTimer > 1.2f) {
                                s.evo = EVO_IDLE;
                                printf("[BlackHole] Star %d settled as mini black hole\n", i);
                            }
                        }
                    }
                    continue; // skip normal lifecycle during evolution
                }

                // ===== Mini BH orbital physics (follows mouse + gravitationally attracted to main BH) =====
                if (s.stage == STAGE_BLACK_HOLE && s.evo == EVO_IDLE) {
                    // ===== Black hole merger phases =====
                    if (s.mergerPhase == MERGER_INSPIRAL) {
                        s.mergerTimer += dt;
                        float progress = s.mergerTimer / config.mergerInspiralDur;
                        progress = fminf(progress, 1.0f);

                        // Steep orbital decay: r(t) = r0 * (1-t/T)^0.35
                        // Steeper exponent = slower start, violent end
                        float r = config.mergerInitialRadius *
                                  powf(fmaxf(1.0f - progress, 0.001f), 0.35f);

                        // Angular frequency: starts slow, explodes at end
                        // omega ~ r^(-1.5), but boost near end for dramatic acceleration
                        float omega = 0.3f * powf(config.mergerInitialRadius / fmaxf(r, 0.003f), 1.5f);
                        // Extra acceleration near coalescence (even stronger)
                        if (progress > 0.6f) {
                            omega *= 1.0f + (progress - 0.6f) * 8.0f;  // 8x boost at end
                        }
                        if (progress > 0.85f) {
                            omega *= 1.0f + (progress - 0.85f) * 15.0f; // Extra kick at final moment
                        }
                        s.mergerOrbitAngle += omega * dt;
                        s.mergerGWFreq = omega;

                        // Both BHs move toward center of mass
                        // Mini BH moves more (lighter), main BH moves less (heavier)
                        // Assume mass ratio ~ 1:3 (mini:main)
                        float miniFrac = 0.75f;  // mini BH moves 75% of the gap
                        float mainFrac = 0.25f;  // main BH moves 25% of the gap

                        // Position: spiral around center of mass (use stored start position)
                        float centerX = s.mergerMainBHStartX;
                        float centerY = s.mergerMainBHStartY;
                        s.x = centerX + r * miniFrac * cosf(s.mergerOrbitAngle);
                        s.y = centerY + r * miniFrac * sinf(s.mergerOrbitAngle);

                        // Main BH offset (opposite direction, smaller)
                        s.mergerMainBHOffsetX = -r * mainFrac * cosf(s.mergerOrbitAngle);
                        s.mergerMainBHOffsetY = -r * mainFrac * sinf(s.mergerOrbitAngle);

                        // GW amplitude grows dramatically as orbit shrinks
                        s.mergerRingdownAmp = powf(progress, 1.5f);

                        // Transition to merger phase
                        if (progress >= 1.0f) {
                            s.mergerPhase = MERGER_MERGER;
                            s.mergerTimer = 0.0f;
                            s.mergerFlash = 1.0f;
                            s.mergerBaseSize = s.size;
                            printf("[BlackHole] Merger phase: coalescence\n");
                        }
                        continue;
                    }
                    else if (s.mergerPhase == MERGER_MERGER) {
                        s.mergerTimer += dt;
                        float progress = s.mergerTimer / config.mergerMergerDur;
                        progress = fminf(progress, 1.0f);

                        // Mini BH shrinks rapidly and gets absorbed
                        s.size = s.mergerBaseSize * (1.0f - progress * 0.95f);
                        s.bhFormationProgress = fmaxf(0.0f, 1.0f - progress * 1.8f);

                        // Main BH moves to meet mini BH
                        s.mergerMainBHOffsetX *= (1.0f - progress * 3.0f);
                        s.mergerMainBHOffsetY *= (1.0f - progress * 3.0f);

                        // Violent flash: huge burst at start, then decay
                        if (progress < 0.15f) {
                            // Explosive burst
                            s.mergerFlash = progress / 0.15f;
                        } else {
                            // Exponential decay
                            s.mergerFlash = expf(-(progress - 0.15f) * 4.0f);
                        }

                        // GW frequency peaks then drops
                        s.mergerGWFreq = 25.0f * expf(-progress * 2.0f) + 5.0f;

                        // Transition to ringdown
                        if (progress >= 1.0f) {
                            s.mergerPhase = MERGER_RINGDOWN;
                            s.mergerTimer = 0.0f;
                            s.mergerRingdownAmp = 1.0f;
                            printf("[BlackHole] Ringdown phase: quasi-normal oscillation\n");
                        }
                        continue;
                    }
                    else if (s.mergerPhase == MERGER_RINGDOWN) {
                        s.mergerTimer += dt;
                        float progress = s.mergerTimer / config.mergerRingdownDur;
                        progress = fminf(progress, 1.0f);

                        // Quasi-Normal Mode: A(t) = exp(-t/tau) * cos(omega_QNM * t)
                        float tau = config.mergerRingdownDur * 0.3f;
                        float omegaQNM = 12.0f;
                        s.mergerRingdownAmp = expf(-s.mergerTimer / tau) *
                                              cosf(omegaQNM * s.mergerTimer);
                        s.mergerFlash = expf(-s.mergerTimer / tau) * 0.3f;
                        s.mergerGWFreq = omegaQNM;

                        // Mini BH fully absorbed, main BH settles back
                        s.size = 0.0f;
                        s.bhFormationProgress = 0.0f;
                        s.mergerMainBHOffsetX *= expf(-dt * 3.0f);
                        s.mergerMainBHOffsetY *= expf(-dt * 3.0f);

                        // Merger complete
                        if (progress >= 1.0f) {
                            s.mergerPhase = MERGER_DONE;
                            s.mergerFlash = 0.0f;
                            s.mergerRingdownAmp = 0.0f;
                            s.mergerMainBHOffsetX = 0.0f;
                            s.mergerMainBHOffsetY = 0.0f;
                            s.state = STAR_DEAD;
                            s.alpha = 0.0f;
                            printf("[BlackHole] Merger complete: mini BH absorbed\n");
                        }
                        continue;
                    }

                    // Normal dual BH orbital physics (no merger active)
                    if (config.dualBHEnabled) {
                        // Spring force toward mouse cursor
                        float dx = mouseX - s.x;
                        float dy = mouseY - s.y;
                        s.vx += dx * config.miniBHMouseFollow * dt;
                        s.vy += dy * config.miniBHMouseFollow * dt;

                        // Gravitational pull from main BH (inverse-square law)
                        float bhDx = bhCx - s.x;
                        float bhDy = bhCy - s.y;
                        float bhDist2 = bhDx * bhDx + bhDy * bhDy;
                        float bhDist = sqrtf(bhDist2);
                        if (bhDist > 0.01f && bhDist < 0.5f) {
                            float grav = config.miniBHGravity / (bhDist2 + 0.001f);
                            s.vx += (bhDx / bhDist) * grav * dt;
                            s.vy += (bhDy / bhDist) * grav * dt;
                        }

                        // Frame-rate-independent velocity damping
                        float dampFactor = expf(-config.miniBHDamping * dt);
                        s.vx *= dampFactor;
                        s.vy *= dampFactor;

                        // Semi-implicit Euler integration
                        s.x += s.vx * dt;
                        s.y += s.vy * dt;

                        // Bounce off screen edges
                        if (s.x < 0.0f) { s.x = 0.0f; s.vx *= -0.5f; }
                        if (s.x > 1.0f) { s.x = 1.0f; s.vx *= -0.5f; }
                        if (s.y < 0.0f) { s.y = 0.0f; s.vy *= -0.5f; }
                        if (s.y > 1.0f) { s.y = 1.0f; s.vy *= -0.5f; }
                    } else {
                        // Simple lerp follow (no physics) when dual BH disabled
                        s.x += (mouseX - s.x) * 5.0f * dt;
                        s.y += (mouseY - s.y) * 5.0f * dt;
                    }
                    continue;
                }

                // Stable evolved stars (WD/NS) stay put, don't follow mouse or fade
                if (s.stage != STAGE_NORMAL) {
                    continue;
                }

                // ===== Normal star lifecycle (stage NORMAL, evo IDLE) =====
                if (s.state == STAR_GROWING) {
                    // Grow while mouse is held
                    s.size += config.starGrowthSpeed * dt;
                    if (s.size > config.starMaxSize) s.size = config.starMaxSize;
                    // Follow mouse
                    s.x += (mouseX - s.x) * 5.0f * dt;
                    s.y += (mouseY - s.y) * 5.0f * dt;
                    // Transition to ALIVE when size reaches a threshold
                    if (s.size >= config.starMaxSize * 0.5f) s.state = STAR_ALIVE;
                } else if (s.state == STAR_ALIVE) {
                    // Continue growing slowly
                    s.size += config.starGrowthSpeed * 0.3f * dt;
                    if (s.size > config.starMaxSize) s.size = config.starMaxSize;
                    // Follow mouse
                    s.x += (mouseX - s.x) * 5.0f * dt;
                    s.y += (mouseY - s.y) * 5.0f * dt;
                    // Start fading when mouse released
                    if (!mouseDown) s.state = STAR_FADING;
                } else if (s.state == STAR_FADING) {
                    // Fade out
                    s.alpha -= config.starFadeSpeed * dt;
                    if (s.alpha <= 0.0f) {
                        s.alpha = 0.0f;
                        s.state = STAR_DEAD;
                        printf("[BlackHole] Star %d died (age=%.1fs)\n", i, s.age);
                    }
                }
            }

            // Compute shared collapse effects (shake/vignette/tension) from active evolution
            {
                float maxCollapse = 0.0f;   // strongest collapse progress
                float supernova = 0.0f;     // supernova progress if active
                for (int i = 0; i < s_starCount; i++) {
                    StarObj& s = s_stars[i];
                    if (s.state == STAR_DEAD) continue;
                    if (s.collapseProgress > maxCollapse) maxCollapse = s.collapseProgress;
                    if (s.supernovaProgress > supernova) supernova = s.supernovaProgress;
                    // Pre-supernova buildup tension
                    if (s.evo == EVO_PRE_SUPERNOVA) maxCollapse = fmaxf(maxCollapse, 0.8f);
                }
                float tensionBase = fmaxf(maxCollapse, supernova > 0.0f ? 1.0f : 0.0f);
                // Supernova moment: peak shake/vignette
                float supernovaPeak = 0.0f;
                if (supernova > 0.0f) {
                    // peak at start of supernova, decays
                    supernovaPeak = (1.0f - supernova) * 1.0f + 0.3f;
                }
                float intensity = fmaxf(maxCollapse * 0.7f, supernovaPeak);
                intensity = fminf(intensity, 1.0f);

                s_evolveShake = intensity * config.collapseShake;
                s_evolveVignette = intensity * config.collapseVignette;
                s_evolveTension = tensionBase * config.collapseTension;

                // Compute pixel shake offset (decays to 0 when no intensity)
                if (s_evolveShake > 0.001f) {
                    float amp = s_evolveShake * 6.0f; // up to ~6 pixels
                    s_shakeX = (RandomFloat() * 2.0f - 1.0f) * amp;
                    s_shakeY = (RandomFloat() * 2.0f - 1.0f) * amp;
                } else {
                    s_shakeX = 0.0f;
                    s_shakeY = 0.0f;
                }
            }

            mouseWasDown = mouseDown;

            // Emit and update solar wind particles
            if (windProgram && config.starWindEnabled) {
                for (int i = 0; i < s_starCount; i++) {
                    StarObj& s = s_stars[i];
                    if (s.state == STAR_GROWING || s.state == STAR_ALIVE) {
                        EmitWindParticles(s, dt, config.starWindCount);
                    }
                }
                UpdateWindParticles(dt, bhCx, bhCy, config.particleGravity);
            }

            // Normal alpha blending for solid star body
            glEnable(0x0BE2/*GL_BLEND*/);
            glBlendFunc(0x0302/*GL_SRC_ALPHA*/, 0x0303/*GL_ONE_MINUS_SRC_ALPHA*/);
            glDisable(0x0B71/*GL_DEPTH_TEST*/);

            // Render each active star
            glUseProgram(starProgram);

            // Bind sun texture
            if (sunTex.valid && suStarTex >= 0) {
                glActiveTexture(0x84C0/*GL_TEXTURE0*/);
                glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, sunTex.texID);
                glUniform1i(suStarTex, 0);
            }

            // Bind framebuffer capture texture to texture unit 1 (for mini BH lensing)
            bool hasMiniBH = false;
            for (int i = 0; i < s_starCount; i++) {
                if (s_stars[i].stage == STAGE_BLACK_HOLE && s_stars[i].state != STAR_DEAD) {
                    hasMiniBH = true;
                    break;
                }
            }
            if (fbCaptureAllocated && suFBTexture >= 0) {
                glActiveTexture(0x84C1/*GL_TEXTURE1*/);
                glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, fbCaptureTex);
                glUniform1i(suFBTexture, 1);
                glActiveTexture(0x84C0/*GL_TEXTURE0*/);
            }

            if (suResolution >= 0) glUniform2f(suResolution, (float)wgl.width, (float)wgl.height);

            double starTime = Win32GL_GetTime(&wgl);

            for (int i = 0; i < s_starCount; i++) {
                StarObj& s = s_stars[i];
                if (s.state == STAR_DEAD) continue;

                // Color shift over time
                float phase = s.colorPhase + s.age * config.starColorShift;
                float shiftR = sinf(phase) * 0.1f;
                float shiftG = sinf(phase + 2.094f) * 0.08f;
                float shiftB = sinf(phase + 4.189f) * 0.1f;
                float cr = fmaxf(0.0f, fminf(1.0f, s.baseR + shiftR));
                float cg = fmaxf(0.0f, fminf(1.0f, s.baseG + shiftG));
                float cb = fmaxf(0.0f, fminf(1.0f, s.baseB + shiftB));

                if (suStarPos >= 0)    glUniform2f(suStarPos, s.x, s.y);
                if (suStarSize >= 0)   glUniform1f(suStarSize, s.size);
                if (suStarAlpha >= 0)  glUniform1f(suStarAlpha, s.alpha);
                if (suStarColor >= 0)  glUniform3f(suStarColor, cr, cg, cb);
                if (suTime >= 0)       glUniform1f(suTime, (float)starTime);
                if (suCoronaSize >= 0) glUniform1f(suCoronaSize, config.starCoronaSize);
                if (suCoronaBright >= 0) glUniform1f(suCoronaBright, config.starCoronaBright);
                if (suFlareChance >= 0) glUniform1f(suFlareChance, config.starFlareChance);

                // 潮汐形变：唯一来源是 CPU 积分得到的 s.tidalAmp（有界）。
                // Why: 删除原先按距离即时计算的 uBHInfluence 回退路径——它会在开关关闭时
                //      用"距离"驱动形变，与本轮的"时间积分"语义冲突（双驱动源 = 历史 bug 根因）。
                if (suBHPos >= 0)    glUniform2f(suBHPos, bhCx, bhCy);
                if (suTidalAmp >= 0) glUniform1f(suTidalAmp, s.tidalAmp);

                // 分层体积气体材质（T3~T5）。D1：材质全局常开，tidalEnabled 只门控撕裂相关量。
                if (suGasOpacity >= 0)    glUniform1f(suGasOpacity,    config.starGasOpacity);
                if (suChromoScale >= 0)   glUniform1f(suChromoScale,   config.starChromoScale);
                if (suCoronaFalloff >= 0) glUniform1f(suCoronaFalloff, config.starCoronaFalloff);
                if (suStreakAmount >= 0)  glUniform1f(suStreakAmount,  config.starStreakAmount);
                if (suStreakSpeed >= 0)   glUniform1f(suStreakSpeed,   config.starStreakSpeed);
                // 撕裂相关量：仅普通恒星 + 开关开启时生效，否则传 0 → 形变/拉丝/消散全部关闭（G9）
                bool matTidal = config.tidalEnabled && s.stage == STAGE_NORMAL;
                if (suTidalProgress >= 0) glUniform1f(suTidalProgress, matTidal ? s.tidalProgress : 0.0f);
                if (suTidalStrip >= 0)    glUniform1f(suTidalStrip,    matTidal ? s.tidalStrip    : 0.0f);
                if (suTidalBodyFade >= 0) glUniform1f(suTidalBodyFade,
                                                      matTidal ? config.tidalBodyFade : 0.0f);

                // ===== Evolution uniforms =====
                if (suStarStage >= 0)            glUniform1i(suStarStage, (int)s.stage);
                if (suCollapseProgress >= 0)     glUniform1f(suCollapseProgress, s.collapseProgress);
                if (suSupernovaProgress >= 0)    glUniform1f(suSupernovaProgress, s.supernovaProgress);
                if (suExpandProgress >= 0)       glUniform1f(suExpandProgress, s.expandProgress);
                if (suCollapseTension >= 0)      glUniform1f(suCollapseTension, s_evolveTension);
                if (suWDBright >= 0)             glUniform1f(suWDBright, config.wdBrightness);
                if (suWDCrystal >= 0)            glUniform1f(suWDCrystal, config.wdCrystalDetail);
                if (suNSRotation >= 0)           glUniform1f(suNSRotation, s.nsRotation);
                if (suNSMagAxisTilt >= 0)        glUniform1f(suNSMagAxisTilt, s.magAxisTilt);
                if (suNSJetLen >= 0)             glUniform1f(suNSJetLen, config.nsJetLength);
                if (suNSJetBright >= 0)          glUniform1f(suNSJetBright, config.nsJetBrightness);
                if (suNSPulseRate >= 0)          glUniform1f(suNSPulseRate, config.nsPulseRate);
                if (suBHFormationProgress >= 0)  glUniform1f(suBHFormationProgress, s.bhFormationProgress);

                // Dual BH + lensing uniforms
                if (suMiniBHRingBright >= 0)      glUniform1f(suMiniBHRingBright, config.miniBHRingBright);
                if (suMiniBHLensStrength >= 0)   glUniform1f(suMiniBHLensStrength, config.miniBHLensStrength);
                if (suDualBHActive >= 0)         glUniform1f(suDualBHActive, (hasMiniBH && config.dualBHEnabled) ? 1.0f : 0.0f);
                if (suGWRippleStrength >= 0)     glUniform1f(suGWRippleStrength, config.gwRippleStrength);
                if (suAccretionBridgeBright >= 0) glUniform1f(suAccretionBridgeBright, config.accretionBridgeBright);
                if (suMiniBHDiskIncl >= 0)       glUniform1f(suMiniBHDiskIncl, config.miniBHDiskIncl);

                // ===== Body pass (alpha blend) =====
                if (suPassMode >= 0) glUniform1i(suPassMode, 0);
                glBindVertexArray(vao);
                glDrawArrays(0x0005/*GL_TRIANGLE_STRIP*/, 0, 4);
                glBindVertexArray(0);

                // ===== Emissive pass (additive blend): jets / supernova / collapse glow =====
                bool needEmissive = (s.stage == STAGE_NEUTRON_STAR)
                                  || (s.stage == STAGE_BLACK_HOLE && config.dualBHEnabled)
                                  || (s.supernovaProgress >= 0.0f)
                                  || (s.collapseProgress > 0.0f);
                if (needEmissive) {
                    glBlendFunc(0x0001/*GL_ONE*/, 0x0001/*GL_ONE*/);
                    if (suPassMode >= 0) glUniform1i(suPassMode, 1);
                    glBindVertexArray(vao);
                    glDrawArrays(0x0005/*GL_TRIANGLE_STRIP*/, 0, 4);
                    glBindVertexArray(0);
                    glBlendFunc(0x0302/*GL_SRC_ALPHA*/, 0x0303/*GL_ONE_MINUS_SRC_ALPHA*/);
                }
            }

            glUseProgram(0);

            // ---- Render solar wind particles ----
            if (windProgram && config.starWindEnabled) {
                // Build wind particle vertex data
                static float s_windData[MAX_WIND_PARTICLES * 4]; // [x, y, life, speed]
                int windVertCount = 0;
                int maxWindVerts = config.starWindCount;
                if (maxWindVerts > MAX_WIND_PARTICLES) maxWindVerts = MAX_WIND_PARTICLES;
                for (int i = 0; i < maxWindVerts; i++) {
                    WindParticle& wp = s_windParticles[i];
                    if (wp.life <= 0.0f) continue;
                    s_windData[windVertCount * 4 + 0] = wp.x;
                    s_windData[windVertCount * 4 + 1] = wp.y;
                    s_windData[windVertCount * 4 + 2] = wp.life * config.starWindBright;
                    s_windData[windVertCount * 4 + 3] = wp.speed * config.starWindSpeed;
                    windVertCount++;
                }

                if (windVertCount > 0) {
                    glUseProgram(windProgram);
                    // Use the color of the first alive star (or default orange-white)
                    float windR = 1.0f, windG = 0.8f, windB = 0.5f;
                    for (int i = 0; i < s_starCount; i++) {
                        if (s_stars[i].state != STAR_DEAD) {
                            windR = s_stars[i].baseR;
                            windG = s_stars[i].baseG;
                            windB = s_stars[i].baseB;
                            break;
                        }
                    }
                    if (wuWindColor >= 0) glUniform3f(wuWindColor, windR, windG, windB);
                    if (wuWindSize >= 0)  glUniform1f(wuWindSize, 4.0f * config.starWindBright);

                    glEnable(0x0BE2/*GL_BLEND*/);
                    glBlendFunc(0x0302/*GL_SRC_ALPHA*/, 0x0001/*GL_ONE*/);
                    glEnable(0x8642/*GL_PROGRAM_POINT_SIZE*/);

                    glBindBuffer(0x8892/*GL_ARRAY_BUFFER*/, windVbo);
                    glBufferSubData(0x8892/*GL_ARRAY_BUFFER*/, 0,
                                    windVertCount * 4 * sizeof(float),
                                    s_windData);
                    glBindVertexArray(windVao);
                    glDrawArrays(0x0000/*GL_POINTS*/, 0, windVertCount);
                    glBindVertexArray(0);

                    glUseProgram(0);
                }
            }

            // Restore GL state
            glDisable(0x0BE2/*GL_BLEND*/);
            glBlendFunc(0x0302/*GL_SRC_ALPHA*/, 0x0303/*GL_ONE_MINUS_SRC_ALPHA*/);
        }

        // ---- Render energy stream: star → black hole (RENDERED LAST for visual priority) ----
        if (config.starEnabled && config.starInteractEnabled && energyProgram && s_starCount > 0) {
            // Find first active star (prioritize black hole for dual BH energy stream)
            StarObj* activeStar = nullptr;
            for (int i = 0; i < s_starCount; i++) {
                if (s_stars[i].stage == STAGE_BLACK_HOLE && s_stars[i].state != STAR_DEAD) {
                    activeStar = &s_stars[i];
                    break;
                }
            }
            if (!activeStar) {
                for (int i = 0; i < s_starCount; i++) {
                    if (s_stars[i].state == STAR_GROWING || s_stars[i].state == STAR_ALIVE || s_stars[i].state == STAR_FADING) {
                        activeStar = &s_stars[i];
                        break;
                    }
                }
            }

            // Debug: log energy stream rendering status every 2 seconds
            static double lastDebugTime = 0.0;
            if (t - lastDebugTime > 2.0) {
                printf("[BlackHole] Energy stream check: starEnabled=%d interact=%d energyProg=%u starCount=%d activeStar=%p\n",
                       config.starEnabled, config.starInteractEnabled, energyProgram, s_starCount, activeStar);
                if (activeStar) {
                    printf("[BlackHole] Active star: pos=(%.3f,%.3f) state=%d color=(%.2f,%.2f,%.2f)\n",
                           activeStar->x, activeStar->y, activeStar->state, activeStar->baseR, activeStar->baseG, activeStar->baseB);
                }
                lastDebugTime = t;
            }

            if (activeStar && activeStar->state != STAR_DEAD) {
                // Calculate distance from star to black hole
                float dxBH = activeStar->x - bhCx;
                float dyBH = activeStar->y - bhCy;
                float distToBH = sqrtf(dxBH * dxBH + dyBH * dyBH);

                // REMOVED: distance restriction - energy stream always visible when star exists
                float maxDist = 1.5f;
                float baseIntensity = 0.7f;
                float distanceFactor = 1.0f - (distToBH / maxDist) * 0.3f;
                float streamIntensity = baseIntensity + distanceFactor * 0.3f;

                if (distToBH < 0.15f) {
                    float closeBoost = (1.0f - distToBH / 0.15f) * 0.3f;
                    streamIntensity += closeBoost;
                }

                // 潮汐闸门：开启撕裂时，普通恒星的流强度随剥离进度 p 单调增长（有底噪）。
                // Why: baseIntensity 常驻 0.7 会让恒星刚生成、甚至远离黑洞时也拖着一条满长的流；
                //      但直接 *= p 又把撕裂前半程压成 0（G4：p≈0.1 就该看到一条细亮丝）。
                //      故用 0.30 + 0.70·p：起点有可辨识的细丝，随 p 单调变亮。
                // 关闭开关时必须严格等价改动前行为，因此闸门必须受 tidalEnabled 保护
                // （否则 tidalProgress 恒为 0 → 普通恒星的能量流被压到 0.30，违反 G9）。
                if (config.tidalEnabled && activeStar->stage == STAGE_NORMAL) {
                    streamIntensity *= (0.30f + 0.70f * activeStar->tidalProgress);
                }

                streamIntensity *= config.streamBright;

                // BH->BH energy stream is more intense
                if (activeStar->stage == STAGE_BLACK_HOLE) {
                    streamIntensity *= 1.5f;
                }

                // Debug: log when actually rendering
                static int frameCount = 0;
                if (frameCount < 5) {
                    printf("[BlackHole] Energy stream rendering: intensity=%.3f starPos=(%.3f,%.3f) bhPos=(%.3f,%.3f) dist=%.3f\n",
                           streamIntensity, activeStar->x, activeStar->y, bhCx, bhCy, distToBH);
                    frameCount++;
                }

                // Render energy stream with additive blending (highest visual priority)
                if (streamIntensity > 0.01f) {
                    glEnable(0x0BE2/*GL_BLEND*/);
                    glBlendFunc(0x0302/*GL_SRC_ALPHA*/, 0x0001/*GL_ONE*/);
                    glDisable(0x0B71/*GL_DEPTH_TEST*/);

                    glUseProgram(energyProgram);

                    if (euStarPos >= 0)         glUniform2f(euStarPos, activeStar->x, activeStar->y);
                    if (euBHPos >= 0)           glUniform2f(euBHPos, bhCx, bhCy);
                    if (euStreamIntensity >= 0) glUniform1f(euStreamIntensity, streamIntensity);
                    if (euTime >= 0)            glUniform1f(euTime, (float)t);
                    if (euResolution >= 0)      glUniform2f(euResolution, (float)wgl.width, (float)wgl.height);
                    if (euStreamWidth >= 0)     glUniform1f(euStreamWidth, config.streamWidth > 0 ? config.streamWidth : 1.0f);
                    if (euSpiralTurns >= 0)     glUniform1f(euSpiralTurns, config.streamSpiralTurns > 0 ? config.streamSpiralTurns : 2.0f);
                    // Energy stream color by evolution stage
                    float streamR = activeStar->baseR, streamG = activeStar->baseG, streamB = activeStar->baseB;
                    if (activeStar->stage == STAGE_WHITE_DWARF) {
                        streamR = 0.8f; streamG = 0.9f; streamB = 1.0f;   // white-blue
                    } else if (activeStar->stage == STAGE_NEUTRON_STAR) {
                        streamR = 0.6f; streamG = 0.4f; streamB = 1.0f;   // violet-blue
                    } else if (activeStar->stage == STAGE_BLACK_HOLE) {
                        streamR = 0.5f; streamG = 0.6f; streamB = 1.0f;   // blue-white plasma (BH->BH)
                    }
                    if (euStreamColor >= 0)     glUniform3f(euStreamColor, streamR, streamG, streamB);
                    if (euStarAge >= 0)         glUniform1f(euStarAge, activeStar->age);
                    // 潮汐模式仅在普通恒星启用；其余阶段传 -1 / 0 让 shader 走原有回退路径
                    {
                        bool tidalMode = config.tidalEnabled && activeStar->stage == STAGE_NORMAL;
                        if (euTidalProgress >= 0) glUniform1f(euTidalProgress, tidalMode ? activeStar->tidalProgress : -1.0f);
                        if (euTidalAmp >= 0)      glUniform1f(euTidalAmp, tidalMode ? activeStar->tidalAmp : 0.0f);
                        if (euTidalStrip >= 0)    glUniform1f(euTidalStrip, tidalMode ? activeStar->tidalStrip : 0.0f);
                        if (euStarSize >= 0)      glUniform1f(euStarSize, activeStar->size);
                    }

                    // T1/D4：把流的螺旋钉死在 blackhole.glsl 的真实吸积盘上。
                    // blackhole.glsl 内盘半径（aspect 校正 UV）= (r_rs / B_CRIT) * rh，其中 rh = HOLE_RADIUS*sz = bhShadowR。
                    // 用与黑洞 shader 同帧同源的 effectiveDiskInner/Outer，避免流自算尺度产生 2.0~2.6 倍漂移。
                    const float B_CRIT = 2.5980762f;
                    float rinUV       = fmaxf(effectiveDiskInner, 1.6f);
                    float routUV      = fmaxf(effectiveDiskOuter, rinUV + 0.5f);
                    float diskInnerUV = (rinUV  / B_CRIT) * bhShadowR;
                    float diskOuterUV = (routUV / B_CRIT) * bhShadowR;
                    if (euDiskInnerUV >= 0) glUniform1f(euDiskInnerUV, diskInnerUV);
                    if (euDiskOuterUV >= 0) glUniform1f(euDiskOuterUV, diskOuterUV);

                    glBindVertexArray(vao);
                    glDrawArrays(0x0005/*GL_TRIANGLE_STRIP*/, 0, 4);
                    glBindVertexArray(0);

                    glUseProgram(0);

                    glDisable(0x0BE2/*GL_BLEND*/);
                    glBlendFunc(0x0302/*GL_SRC_ALPHA*/, 0x0303/*GL_ONE_MINUS_SRC_ALPHA*/);
                }
            }
        }

        // ---- 窗口吞噬碎片渲染：黑洞场景与能量流之上、ImGui 之下 ----
        // Update 内部自绘（自备 shader/混合/深度状态保存还原），无活动碎片时零开销。
        WindowDevour_Update(holeScreenX, holeScreenY, dt);

        // ---- Middle-click: two-key coexistence (Q5) ----
        // 存在 stage==NEUTRON_STAR 且 evo==EVO_IDLE 的恒星 → 走既有中子星坍缩；
        // 否则 → DesktopIcons_Toggle() 切换图标吞噬。放在恒星模块之外，
        // 保证恒星系统关闭时中键仍可触发吞噬。
        {
            bool mMouseDown = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
            static bool mMouseWasDown = false;
            if (mMouseDown && !mMouseWasDown) {
                bool collapsed = false;
                if (config.starEvolutionEnabled) {
                    for (int i = 0; i < s_starCount; i++) {
                        StarObj& s = s_stars[i];
                        if (s.state == STAR_DEAD) continue;
                        if (s.evo != EVO_IDLE) continue;
                        if (s.stage == STAGE_NEUTRON_STAR) {
                            s.evo = EVO_COLLAPSING;
                            s.evoTimer = 0.0f;
                            s.collapseProgress = 0.0f;
                            s.supernovaProgress = -1.0f;
                            printf("[BlackHole] Star %d: middle-click -> collapse beyond Oppenheimer -> black hole\n", i);
                            collapsed = true;
                            break;
                        }
                    }
                }
                if (!collapsed) {
                    DesktopIcons_Toggle();
                    printf("[BlackHole] Middle-click -> icon devour %s: %s\n",
                           DesktopIcons_IsDevouring() ? "START" : "STOP",
                           DesktopIcons_LastError());
                }
                // 窗口吞噬：与中子星坍缩/图标吞噬并存（计划书 Q1：同一次中键同时触发）。
                // 被吞窗口按用户需求保持隐藏，不提供还原。
                if (config.winDevourEnabled) {
                    int eaten = WindowDevour_Trigger(holeScreenX, holeScreenY);
                    printf("[BlackHole] Middle-click -> window devour: %d windows\n", eaten);
                }
            }
            mMouseWasDown = mMouseDown;
        }

        // Render ImGui panel (on top of black hole)
        GUI_Render(&config);

        Win32GL_SwapBuffers(&wgl);
    }

    // Save config on exit
    Config_Save(&config, "blackhole_config.ini");

    // Cleanup
    if (particleFBOAllocated) {
        if (particleFBO) glDeleteFramebuffers(1, &particleFBO);
        if (particleFBOTex) glDeleteTextures(1, &particleFBOTex);
        particleFBO = 0;
        particleFBOTex = 0;
        particleFBOAllocated = false;
    }
    // 退出还原桌面图标（atexit 之外的主退出路径显式调用）
    DesktopIcons_Shutdown();
    // 窗口吞噬：清理碎片纹理/缓冲（被吞窗口按需求保持隐藏，不还原）
    WindowDevour_Shutdown();
    GUI_Shutdown();
    if (captureActive) WGC_Shutdown(&wgc);
    GLTex_Shutdown(&glTex);
    Win32GL_Shutdown(&wgl);
    printf("[BlackHole] Exited cleanly\n");
    return 0;
}
