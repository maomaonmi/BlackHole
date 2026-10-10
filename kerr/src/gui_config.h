#pragma once

// Config data structures and GUI panel declarations

struct DiskPreset {
    char name[64] = {};
    float diskTemp    = 5500.0f;
    float diskIncl    = 1.5f;
    float diskRoll    = 0.35f;
    float diskInner   = 1.8f;
    float diskOuter   = 8.0f;
    float diskOpac    = 0.9f;
    float dopplerMix  = 0.6f;
    float diskBeam    = 2.5f;
    float diskGain    = 2.2f;
    float diskContrast = 1.6f;
    float diskWind    = 7.0f;
    float diskSpeed   = 5.0f;
    float exposure    = 1.4f;
    float starGain    = 0.0f;
};

struct BlackholeConfig {
    // Kerr metric parameters
    bool  kerrMode       = false;    // Kerr mode on/off
    float spinParam      = 0.0f;     // spin parameter a (0.0 ~ 5.0)
    int   numSteps       = 48;       // geodesic integration steps (48 ~ 256)
    float colorGradient  = 0.0f;     // disk color gradient intensity (0=off, 1=strong)

    // Advanced overrides (-1 = use shader default)
    float holeRadius  = -1.0f;
    float diskGain    = -1.0f;
    float diskTemp    = -1.0f;
    float exposure    = -1.0f;
    float speed       = -1.0f;
    float starGain    = -1.0f;
    float diskIncl    = 1.5f;
    float lensDepth   = -1.0f;
    float dilationMin = -1.0f;

    // Time dilation (1.0 = normal)
    float timeDilation = 1.0f;

    // Relativistic jets
    bool  jetEnabled     = false;
    float jetLength      = 20.0f;
    float jetWidth       = 0.15f;
    float jetSpeed       = 2.0f;
    float jetBrightness  = 1.5f;

    // Particle system
    bool  particleEnabled  = true;
    int   particleCount    = 2000;
    float particleSize     = 6.0f;
    float particleSpiral   = 1.2f;
    float particleGravity  = 2.0f;
    float particleColorR   = 1.0f;
    float particleColorG   = 0.40f;
    float particleColorB   = 0.05f;
    int   particleTrailLength = 40;   // trail segments [5, 60]
    bool  particleCloudEnabled = true; // faint plasma cloud around streams
    // Particle disk tilt angles (radians)
    float particleTiltX    = 1.5f;    // front-back tilt (compress Y)
    float particleTiltY    = 0.0f;    // left-right tilt (compress X)
    float particleTiltZ    = 0.0f;    // rotation around Z axis

    // Supermassive black hole mode (larger accretion disk)
    bool  supermassiveMode = false;

    // Star system (interactive)
    bool  starEnabled        = true;    // star system on/off
    float starMaxSize        = 0.25f;   // max star radius (UV proportion)
    float starGrowthSpeed    = 0.06f;   // grow speed while holding mouse
    float starFadeSpeed      = 1.2f;    // fade speed on release
    float starCoronaSize     = 2.0f;    // corona radius multiplier
    float starCoronaBright   = 0.7f;    // corona brightness
    float starFlareChance    = 0.5f;    // flare intensity
    float starColorShift     = 0.15f;   // color shift rate over time
    bool  starWindEnabled    = true;    // solar wind particles on/off
    float starWindSpeed      = 1.0f;    // solar wind emission speed
    int   starWindCount      = 1500;    // max solar wind particles
    float starWindBright     = 0.8f;    // solar wind brightness
    float starLensStrength   = 0.3f;    // gravitational lensing strength

    // Star-BH interaction (energy stream)
    bool  starInteractEnabled = true;    // star-BH interaction on/off
    float starAbsorbRadius    = 0.25f;   // absorption trigger radius
    float starAbsorbSpeed     = 0.03f;   // star shrink speed during absorption
    float streamBright        = 2.0f;    // energy stream brightness multiplier
    float streamWidth         = 1.0f;    // energy stream width multiplier
    float streamSpiralTurns   = 2.0f;    // spiral turns around BH

    // Tidal disruption (恒星潮汐撕裂)
    // Why: 本体形变与能量流必须由同一物理量驱动，否则两者形态互相独立（历史 bug）
    bool  tidalEnabled        = true;    // 总开关：关闭 → p/amp/strip 全为 0，严格等价改动前行为
    float tidalRadius         = 0.30f;   // 潮汐半径 r_t (UV)：仅 r < r_t 进入撕裂区
    float tidalDisruptSec     = 5.0f;    // 恰好落在 r_t 时从 0 剥到 1 所需秒数（时间量纲锚点）
    float tidalBetaMax        = 3.0f;    // 穿透因子上限 β_max：β = r_t/r 的截断（决定最快撕裂速度）
    float tidalStretch        = 0.6f;    // 本体最大伸长率 amp_body|max（有界；不再随 p 发散）
    float tidalBodyFade       = 1.0f;    // 本体消散强度 [0,1]：α_body = 1 - fade*smoothstep(0.35,1,p)
    float tidalLinkAccretion  = 1.0f;    // 吸积盘/喷流联动增益 L：[0,1]，=0 则关闭联动

    // Stellar evolution (right-click triggered: star -> white dwarf -> neutron star -> black hole)
    bool  starEvolutionEnabled = true;   // right-click evolution on/off
    float evoExpandSpeed       = 0.25f;  // first right-click expansion speed
    float evoCollapseSpeed     = 0.35f;  // collapse speed (size shrink rate)
    float supernovaBrightness  = 2.5f;   // supernova flash brightness
    float supernovaDuration    = 1.2f;   // supernova explosion duration (seconds)
    float collapseShake        = 0.8f;   // screen shake intensity during collapse
    float collapseVignette     = 0.7f;   // screen-edge darkening during collapse
    float collapseTension       = 0.6f;   // fine tension lines during collapse
    // White dwarf
    float wdBrightness         = 1.6f;   // white dwarf glow brightness
    float wdCrystalDetail      = 7.0f;   // crystal texture detail (noise octaves)
    // Neutron star
    float nsRotationSpeed      = 6.0f;   // neutron star rotation speed
    float nsJetLength          = 0.9f;   // polar jet length
    float nsJetBrightness      = 2.0f;   // polar jet brightness
    float nsAxisTilt           = 0.6f;   // magnetic axis tilt (rad)
    float nsPulseRate          = 3.0f;   // pulsar lighthouse sweep rate
    // Mini black hole (final stage)
    float miniBHRingBright      = 1.5f;   // photon ring brightness
    // Dual black hole system (mini BH <-> main BH interactions)
    bool  dualBHEnabled          = true;
    float miniBHDiskIncl          = 1.2f;   // accretion disk inclination angle (rad, 0=face-on)
    float miniBHMouseFollow      = 8.0f;   // mouse-follow spring constant
    float miniBHGravity          = 0.002f; // gravitational attraction to main BH
    float miniBHDamping          = 2.0f;   // velocity damping rate
    float miniBHLensStrength     = 3.0f;   // gravitational lensing strength
    float gwRippleStrength       = 0.5f;   // gravitational wave ripple intensity
    float accretionBridgeBright  = 1.0f;   // accretion bridge brightness

    // Black hole merger (mini BH + main BH coalescence)
    bool  mergerEnabled         = true;    // merger feature on/off
    float mergerInspiralDur     = 12.0f;   // inspiral duration (seconds)
    float mergerMergerDur       = 1.5f;    // merger duration (seconds)
    float mergerRingdownDur     = 6.0f;    // ringdown duration (seconds)
    float mergerInitialRadius   = 0.25f;   // initial orbit radius (UV)
    float mergerFlashBright     = 2.0f;    // merger flash brightness
    float mergerGWBurst         = 3.0f;    // GW burst strength

    // ---- Desktop icon devour（桌面图标漩涡吞噬）----
    bool  iconDevourEnabled   = false;   // 功能总开关
    float iconDevourRate      = 0.35f;   // 内缩速率 λ
    float iconDevourSpin      = 1.2f;    // 旋转角速度 ω
    float iconDevourStretch   = 2.0f;    // shader 径向拉伸增益（越大越接近"一根线"）
    float iconDevourSwirl     = 1.0f;    // shader 漩涡增益
    float iconDevourStagger   = 0.15f;   // 错峰间隔（秒 / 图标）
    int   iconDevourThrottleHz= 30;      // 坐标下发频率
    bool  iconDevourRestore   = true;    // 停止时是否还原

    // Smoothed values (lerped toward target each frame)
    float smoothHoleRadius = -1.0f;
    float smoothDiskGain   = -1.0f;
    float smoothDiskTemp   = -1.0f;
    float smoothExposure   = -1.0f;
    float smoothSpeed      = -1.0f;
    float smoothStarGain   = -1.0f;
    float smoothDiskIncl   = -1.0f;
    float smoothLensDepth  = -1.0f;
    float smoothDilationMin = -1.0f;

    // Display mode
    int   displayMode   = 1;  // 0=always, 1=idle
    int   idleSeconds   = 300;
    bool  videoAsIdle   = false;

    // Capture
    int   captureMode   = -1; // -1=auto, 0=WGC, 1=DXGI

    // Monitor
    int   monitorIndex  = 0;  // 0=primary

    // Playback
    int   playMode      = 0;  // 0=sequential, 1=cyclic, 2=random
    float slotSec       = 5.25f;

    // Fixed size
    bool  fixedSize     = false;
    float fixedLevel    = 1.0f;

    // Demo tour mode (automatic preset cycling)
    bool  demoMode      = true;

    // Auto start
    bool  autoStart     = false;

    // Presets
    static const int MAX_PRESETS = 64;
    int   presetCount = 0;
    DiskPreset presets[MAX_PRESETS] = {};

    // Full disk parameters (for advanced control)
    float diskRoll      = -1.0f;
    float diskInner     = -1.0f;
    float diskOuter     = -1.0f;
    float diskOpac      = -1.0f;
    float dopplerMix    = -1.0f;
    float diskBeam      = -1.0f;
    float diskContrast  = -1.0f;
    float diskWind      = -1.0f;
    float diskSpeed     = -1.0f;
};

// Save/Load config (JSON format)
bool Config_Load(BlackholeConfig* cfg, const char* path);
bool Config_Save(const BlackholeConfig* cfg, const char* path);
void Config_SetDefaults(BlackholeConfig* cfg);

// Initialize built-in presets
void Config_InitBuiltInPresets(BlackholeConfig* cfg);

// Smooth parameter transitions
void Config_SmoothUpdate(BlackholeConfig* cfg, float dt);

// ImGui panel
struct Win32GL;
void GUI_Init(Win32GL* wgl);
void GUI_Shutdown();
void GUI_Render(BlackholeConfig* cfg);
bool GUI_IsActive();
void GUI_SetActive(bool active);
