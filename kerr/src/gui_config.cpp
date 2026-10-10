#include "gui_config.h"
#include "win32_gl.h"
#include "desktop_icons.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sstream>

// ---- Built-in black hole presets ----
static const DiskPreset s_builtInPresets[] = {
    { u8"\u4EBA\u9A6C\u5EA7 A*",         3500, 0.8f, 0.2f, 2.0f, 10.0f, 0.95f, 0.5f, 1.8f, 2.0f, 1.4f, 4.0f, 3.0f, 1.0f, 0.3f },
    { u8"M87*",                          4500, 2.8f, 0.5f, 2.5f, 12.0f, 0.85f, 0.7f, 3.0f, 2.5f, 1.7f, 5.0f, 4.0f, 1.2f, 0.1f },
    { u8"\u5929\u9E45\u5EA7 X-1",        8000, 1.2f, 0.4f, 1.5f, 6.0f,  0.8f,  0.8f, 2.0f, 3.0f, 1.8f, 8.0f, 6.0f, 1.5f, 0.5f },
    { u8"\u5361\u5C14\u5188\u56FE\u96C5 (\u661F\u9645\u7A7F\u8D8A)",  3000, 1.5f, 0.35f, 1.8f, 8.0f, 0.9f, 0.6f, 2.5f, 2.2f, 1.6f, 7.0f, 5.0f, 1.4f, 0.0f },
    { u8"\u5FAE\u578B\u9ED1\u6D1E",    15000, 0.5f, 0.1f, 0.8f, 3.0f,  0.7f,  0.9f, 1.5f, 4.0f, 2.0f, 12.0f, 10.0f, 2.0f, 1.0f },
    { u8"\u539F\u521D\u9ED1\u6D1E",    20000, 0.3f, 0.05f, 0.5f, 2.0f, 0.6f, 1.0f, 1.0f, 5.0f, 2.5f, 15.0f, 12.0f, 3.0f, 2.0f },
};
static const int s_builtInCount = sizeof(s_builtInPresets) / sizeof(s_builtInPresets[0]);

// ---- GUI state ----
static bool s_guiActive = false;
static int  s_selectedPreset = -1;
static int  s_selectedBuiltIn = -1;
static bool s_showAdvanced = false;
static char s_newPresetName[64] = {};

// ---- Config defaults ----
void Config_SetDefaults(BlackholeConfig* cfg) {
    if (!cfg) return;
    cfg->kerrMode       = false;
    cfg->spinParam      = 0.0f;
    cfg->numSteps       = 48;
    cfg->colorGradient  = 0.0f;
    cfg->holeRadius  = -1.0f;
    cfg->diskGain    = -1.0f;
    cfg->diskTemp    = -1.0f;
    cfg->exposure    = -1.0f;
    cfg->speed       = -1.0f;
    cfg->starGain    = -1.0f;
    cfg->diskIncl    = 1.5f;
    cfg->smoothHoleRadius = -1.0f;
    cfg->smoothDiskGain   = -1.0f;
    cfg->smoothDiskTemp   = -1.0f;
    cfg->smoothExposure   = -1.0f;
    cfg->smoothSpeed      = -1.0f;
    cfg->smoothStarGain   = -1.0f;
    cfg->smoothDiskIncl   = -1.0f;
    cfg->displayMode = 1;
    cfg->idleSeconds = 300;
    cfg->videoAsIdle = false;
    cfg->captureMode = -1;
    cfg->monitorIndex = 0;
    cfg->playMode    = 0;
    cfg->slotSec     = 5.25f;
    cfg->fixedSize   = false;
    cfg->fixedLevel  = 1.0f;
    cfg->demoMode    = true;
    cfg->autoStart   = false;
    cfg->presetCount = 0;
    cfg->diskRoll      = -1.0f;
    cfg->diskInner     = -1.0f;
    cfg->diskOuter     = -1.0f;
    cfg->diskOpac      = -1.0f;
    cfg->dopplerMix    = -1.0f;
    cfg->diskBeam      = -1.0f;
    cfg->diskContrast  = -1.0f;
    cfg->diskWind      = -1.0f;
    cfg->diskSpeed     = -1.0f;
    cfg->lensDepth     = -1.0f;
    cfg->dilationMin   = -1.0f;
    cfg->timeDilation    = 1.0f;
    cfg->jetEnabled      = false;
    cfg->jetLength       = 20.0f;
    cfg->jetWidth        = 0.15f;
    cfg->jetSpeed        = 2.0f;
    cfg->jetBrightness   = 1.5f;
    cfg->particleEnabled  = true;
    cfg->particleCount    = 2000;
    cfg->particleSize     = 6.0f;
    cfg->particleSpiral   = 1.2f;
    cfg->particleGravity  = 2.0f;
    cfg->particleColorR   = 1.0f;
    cfg->particleColorG   = 0.40f;
    cfg->particleColorB   = 0.05f;
    cfg->particleTrailLength = 40;
    cfg->particleCloudEnabled = true;
    cfg->particleTiltX = 1.5f;
    cfg->particleTiltY = 0.0f;
    cfg->particleTiltZ = 0.0f;
    cfg->supermassiveMode = false;
    cfg->starEnabled        = true;
    cfg->starMaxSize        = 0.25f;
    cfg->starGrowthSpeed    = 0.06f;
    cfg->starFadeSpeed      = 1.2f;
    cfg->starCoronaSize     = 2.0f;
    cfg->starCoronaBright   = 0.7f;
    cfg->starFlareChance    = 0.5f;
    cfg->starColorShift     = 0.15f;
    cfg->starWindEnabled    = true;
    cfg->starWindSpeed      = 1.0f;
    cfg->starWindCount      = 1500;
    cfg->starWindBright     = 0.8f;
    cfg->starLensStrength   = 0.3f;
    cfg->starInteractEnabled = true;
    cfg->starAbsorbRadius    = 0.25f;
    cfg->starAbsorbSpeed     = 0.03f;
    cfg->streamBright        = 2.0f;
    cfg->streamWidth         = 1.0f;
    cfg->streamSpiralTurns   = 2.0f;
    cfg->tidalEnabled        = true;
    cfg->tidalRadius         = 0.30f;
    cfg->tidalRate           = 1.2f;
    cfg->tidalStretch        = 1.0f;

    // Stellar evolution defaults
    cfg->starEvolutionEnabled = true;
    cfg->evoExpandSpeed       = 0.25f;
    cfg->evoCollapseSpeed     = 0.35f;
    cfg->supernovaBrightness  = 2.5f;
    cfg->supernovaDuration    = 1.2f;
    cfg->collapseShake        = 0.8f;
    cfg->collapseVignette     = 0.7f;
    cfg->collapseTension       = 0.6f;
    cfg->wdBrightness         = 1.6f;
    cfg->wdCrystalDetail      = 7.0f;
    cfg->nsRotationSpeed      = 6.0f;
    cfg->nsJetLength          = 0.9f;
    cfg->nsJetBrightness      = 2.0f;
    cfg->nsAxisTilt           = 0.6f;
    cfg->nsPulseRate          = 3.0f;
    cfg->miniBHRingBright      = 1.5f;
    cfg->dualBHEnabled         = true;
    cfg->miniBHMouseFollow     = 8.0f;
    cfg->miniBHGravity         = 0.002f;
    cfg->miniBHDamping         = 2.0f;
    cfg->miniBHDiskIncl        = 1.2f;   // ~70 degrees, nice 3D perspective
    cfg->miniBHLensStrength    = 3.0f;
    cfg->gwRippleStrength      = 0.5f;
    cfg->accretionBridgeBright = 1.0f;

    cfg->mergerEnabled         = true;
    cfg->mergerInspiralDur     = 12.0f;
    cfg->mergerMergerDur       = 1.5f;
    cfg->mergerRingdownDur     = 6.0f;
    cfg->mergerInitialRadius   = 0.25f;
    cfg->mergerFlashBright     = 2.0f;
    cfg->mergerGWBurst         = 3.0f;

    // Desktop icon devour defaults
    cfg->iconDevourEnabled    = false;
    cfg->iconDevourRate       = 0.35f;
    cfg->iconDevourSpin       = 1.2f;
    cfg->iconDevourStretch    = 2.0f;
    cfg->iconDevourSwirl      = 1.0f;
    cfg->iconDevourStagger    = 0.15f;
    cfg->iconDevourThrottleHz = 30;
    cfg->iconDevourRestore    = true;
}

void Config_InitBuiltInPresets(BlackholeConfig* cfg) {
    if (!cfg) return;
    // Copy built-in presets into config
    int count = s_builtInCount;
    if (count > BlackholeConfig::MAX_PRESETS) count = BlackholeConfig::MAX_PRESETS;
    cfg->presetCount = count;
    for (int i = 0; i < count; i++) {
        cfg->presets[i] = s_builtInPresets[i];
    }
}

// ---- Smooth parameter transitions ----
static float SmoothStep(float current, float target, float dt, float speed = 8.0f) {
    if (current == target) return target;
    float diff = target - current;
    float step = diff * (1.0f - expf(-speed * dt));
    if (fabsf(diff) < 0.0001f) return target;
    return current + step;
}

void Config_SmoothUpdate(BlackholeConfig* cfg, float dt) {
    if (!cfg) return;
    cfg->smoothHoleRadius = SmoothStep(cfg->smoothHoleRadius, cfg->holeRadius, dt);
    cfg->smoothDiskGain   = SmoothStep(cfg->smoothDiskGain,   cfg->diskGain,   dt);
    cfg->smoothDiskTemp   = SmoothStep(cfg->smoothDiskTemp,   cfg->diskTemp,   dt);
    cfg->smoothExposure   = SmoothStep(cfg->smoothExposure,   cfg->exposure,   dt);
    cfg->smoothSpeed      = SmoothStep(cfg->smoothSpeed,      cfg->speed,      dt);
    cfg->smoothStarGain   = SmoothStep(cfg->smoothStarGain,   cfg->starGain,   dt);
    cfg->smoothDiskIncl   = SmoothStep(cfg->smoothDiskIncl,   cfg->diskIncl,   dt);
}

// ---- Config save/load (simple text format) ----
bool Config_Save(const BlackholeConfig* cfg, const char* path) {
    if (!cfg || !path) return false;
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "[BlackHole Config v1]\n";
    f << "kerrMode=" << (cfg->kerrMode ? 1 : 0) << "\n";
    f << "spinParam=" << cfg->spinParam << "\n";
    f << "numSteps=" << cfg->numSteps << "\n";
    f << "colorGradient=" << cfg->colorGradient << "\n";
    f << "holeRadius=" << cfg->holeRadius << "\n";
    f << "diskGain=" << cfg->diskGain << "\n";
    f << "diskTemp=" << cfg->diskTemp << "\n";
    f << "exposure=" << cfg->exposure << "\n";
    f << "speed=" << cfg->speed << "\n";
    f << "starGain=" << cfg->starGain << "\n";
    f << "diskIncl=" << cfg->diskIncl << "\n";
    f << "diskRoll=" << cfg->diskRoll << "\n";
    f << "diskInner=" << cfg->diskInner << "\n";
    f << "diskOuter=" << cfg->diskOuter << "\n";
    f << "diskOpac=" << cfg->diskOpac << "\n";
    f << "dopplerMix=" << cfg->dopplerMix << "\n";
    f << "diskBeam=" << cfg->diskBeam << "\n";
    f << "diskContrast=" << cfg->diskContrast << "\n";
    f << "diskWind=" << cfg->diskWind << "\n";
    f << "diskSpeed=" << cfg->diskSpeed << "\n";
    f << "lensDepth=" << cfg->lensDepth << "\n";
    f << "dilationMin=" << cfg->dilationMin << "\n";
    f << "timeDilation=" << cfg->timeDilation << "\n";
    f << "jetEnabled=" << (cfg->jetEnabled ? 1 : 0) << "\n";
    f << "jetLength=" << cfg->jetLength << "\n";
    f << "jetWidth=" << cfg->jetWidth << "\n";
    f << "jetSpeed=" << cfg->jetSpeed << "\n";
    f << "jetBrightness=" << cfg->jetBrightness << "\n";
    f << "particleEnabled=" << (cfg->particleEnabled ? 1 : 0) << "\n";
    f << "particleCount=" << cfg->particleCount << "\n";
    f << "particleSize=" << cfg->particleSize << "\n";
    f << "particleSpiral=" << cfg->particleSpiral << "\n";
    f << "particleGravity=" << cfg->particleGravity << "\n";
    f << "particleColorR=" << cfg->particleColorR << "\n";
    f << "particleColorG=" << cfg->particleColorG << "\n";
    f << "particleColorB=" << cfg->particleColorB << "\n";
    f << "particleTrailLength=" << cfg->particleTrailLength << "\n";
    f << "particleCloudEnabled=" << (cfg->particleCloudEnabled ? 1 : 0) << "\n";
    f << "particleTiltX=" << cfg->particleTiltX << "\n";
    f << "particleTiltY=" << cfg->particleTiltY << "\n";
    f << "particleTiltZ=" << cfg->particleTiltZ << "\n";
    f << "supermassiveMode=" << (cfg->supermassiveMode ? 1 : 0) << "\n";
    f << "starEnabled=" << (cfg->starEnabled ? 1 : 0) << "\n";
    f << "starMaxSize=" << cfg->starMaxSize << "\n";
    f << "starGrowthSpeed=" << cfg->starGrowthSpeed << "\n";
    f << "starFadeSpeed=" << cfg->starFadeSpeed << "\n";
    f << "starCoronaSize=" << cfg->starCoronaSize << "\n";
    f << "starCoronaBright=" << cfg->starCoronaBright << "\n";
    f << "starFlareChance=" << cfg->starFlareChance << "\n";
    f << "starColorShift=" << cfg->starColorShift << "\n";
    f << "starWindEnabled=" << (cfg->starWindEnabled ? 1 : 0) << "\n";
    f << "starWindSpeed=" << cfg->starWindSpeed << "\n";
    f << "starWindCount=" << cfg->starWindCount << "\n";
    f << "starWindBright=" << cfg->starWindBright << "\n";
    f << "starLensStrength=" << cfg->starLensStrength << "\n";
    f << "starInteractEnabled=" << (cfg->starInteractEnabled ? 1 : 0) << "\n";
    f << "starAbsorbRadius=" << cfg->starAbsorbRadius << "\n";
    f << "starAbsorbSpeed=" << cfg->starAbsorbSpeed << "\n";
    f << "streamBright=" << cfg->streamBright << "\n";
    f << "streamWidth=" << cfg->streamWidth << "\n";
    f << "streamSpiralTurns=" << cfg->streamSpiralTurns << "\n";
    f << "tidalEnabled=" << (cfg->tidalEnabled ? 1 : 0) << "\n";
    f << "tidalRadius=" << cfg->tidalRadius << "\n";
    f << "tidalRate=" << cfg->tidalRate << "\n";
    f << "tidalStretch=" << cfg->tidalStretch << "\n";
    f << "starEvolutionEnabled=" << (cfg->starEvolutionEnabled ? 1 : 0) << "\n";
    f << "evoExpandSpeed=" << cfg->evoExpandSpeed << "\n";
    f << "evoCollapseSpeed=" << cfg->evoCollapseSpeed << "\n";
    f << "supernovaBrightness=" << cfg->supernovaBrightness << "\n";
    f << "supernovaDuration=" << cfg->supernovaDuration << "\n";
    f << "collapseShake=" << cfg->collapseShake << "\n";
    f << "collapseVignette=" << cfg->collapseVignette << "\n";
    f << "collapseTension=" << cfg->collapseTension << "\n";
    f << "wdBrightness=" << cfg->wdBrightness << "\n";
    f << "wdCrystalDetail=" << cfg->wdCrystalDetail << "\n";
    f << "nsRotationSpeed=" << cfg->nsRotationSpeed << "\n";
    f << "nsJetLength=" << cfg->nsJetLength << "\n";
    f << "nsJetBrightness=" << cfg->nsJetBrightness << "\n";
    f << "nsAxisTilt=" << cfg->nsAxisTilt << "\n";
    f << "nsPulseRate=" << cfg->nsPulseRate << "\n";
    f << "miniBHRingBright=" << cfg->miniBHRingBright << "\n";
    f << "dualBHEnabled=" << (cfg->dualBHEnabled ? 1 : 0) << "\n";
    f << "miniBHMouseFollow=" << cfg->miniBHMouseFollow << "\n";
    f << "miniBHGravity=" << cfg->miniBHGravity << "\n";
    f << "miniBHDamping=" << cfg->miniBHDamping << "\n";
    f << "miniBHDiskIncl=" << cfg->miniBHDiskIncl << "\n";
    f << "miniBHLensStrength=" << cfg->miniBHLensStrength << "\n";
    f << "gwRippleStrength=" << cfg->gwRippleStrength << "\n";
    f << "accretionBridgeBright=" << cfg->accretionBridgeBright << "\n";
    f << "mergerEnabled=" << (cfg->mergerEnabled ? 1 : 0) << "\n";
    f << "mergerInspiralDur=" << cfg->mergerInspiralDur << "\n";
    f << "mergerMergerDur=" << cfg->mergerMergerDur << "\n";
    f << "mergerRingdownDur=" << cfg->mergerRingdownDur << "\n";
    f << "mergerInitialRadius=" << cfg->mergerInitialRadius << "\n";
    f << "mergerFlashBright=" << cfg->mergerFlashBright << "\n";
    f << "mergerGWBurst=" << cfg->mergerGWBurst << "\n";
    f << "iconDevourEnabled=" << (cfg->iconDevourEnabled ? 1 : 0) << "\n";
    f << "iconDevourRate=" << cfg->iconDevourRate << "\n";
    f << "iconDevourSpin=" << cfg->iconDevourSpin << "\n";
    f << "iconDevourStretch=" << cfg->iconDevourStretch << "\n";
    f << "iconDevourSwirl=" << cfg->iconDevourSwirl << "\n";
    f << "iconDevourStagger=" << cfg->iconDevourStagger << "\n";
    f << "iconDevourThrottleHz=" << cfg->iconDevourThrottleHz << "\n";
    f << "iconDevourRestore=" << (cfg->iconDevourRestore ? 1 : 0) << "\n";
    f << "displayMode=" << cfg->displayMode << "\n";
    f << "idleSeconds=" << cfg->idleSeconds << "\n";
    f << "playMode=" << cfg->playMode << "\n";
    f << "slotSec=" << cfg->slotSec << "\n";
    f << "fixedSize=" << (cfg->fixedSize ? 1 : 0) << "\n";
    f << "fixedLevel=" << cfg->fixedLevel << "\n";
    f << "demoMode=" << (cfg->demoMode ? 1 : 0) << "\n";
    f << "presetCount=" << cfg->presetCount << "\n";

    for (int i = 0; i < cfg->presetCount; i++) {
        const auto& p = cfg->presets[i];
        f << "preset_" << i << "=" << p.name
          << "," << p.diskTemp << "," << p.diskIncl << "," << p.diskRoll
          << "," << p.diskInner << "," << p.diskOuter << "," << p.diskOpac
          << "," << p.dopplerMix << "," << p.diskBeam << "," << p.diskGain
          << "," << p.diskContrast << "," << p.diskWind << "," << p.diskSpeed
          << "," << p.exposure << "," << p.starGain << "\n";
    }
    return true;
}

static float ParseFloat(const std::string& s, float def = -1.0f) {
    try { return std::stof(s); }
    catch (...) { return def; }
}

static int ParseInt(const std::string& s, int def = 0) {
    try { return std::stoi(s); }
    catch (...) { return def; }
}

bool Config_Load(BlackholeConfig* cfg, const char* path) {
    if (!cfg || !path) return false;
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    std::getline(f, line); // skip header
    while (std::getline(f, line)) {
        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        std::string key = line.substr(0, eqPos);
        std::string val = line.substr(eqPos + 1);

        if (key == "kerrMode")       cfg->kerrMode = ParseInt(val, 0) != 0;
        else if (key == "spinParam")      cfg->spinParam = ParseFloat(val, 0.0f);
        else if (key == "numSteps")       cfg->numSteps = ParseInt(val, 48);
        else if (key == "colorGradient")  cfg->colorGradient = ParseFloat(val, 0.0f);
        else if (key == "holeRadius")  cfg->holeRadius = ParseFloat(val);
        else if (key == "diskGain")    cfg->diskGain = ParseFloat(val);
        else if (key == "diskTemp")    cfg->diskTemp = ParseFloat(val);
        else if (key == "exposure")    cfg->exposure = ParseFloat(val);
        else if (key == "speed")       cfg->speed = ParseFloat(val);
        else if (key == "starGain")    cfg->starGain = ParseFloat(val);
        else if (key == "diskIncl")    cfg->diskIncl = ParseFloat(val, 1.5f);
        else if (key == "diskRoll")    cfg->diskRoll = ParseFloat(val);
        else if (key == "diskInner")   cfg->diskInner = ParseFloat(val);
        else if (key == "diskOuter")   cfg->diskOuter = ParseFloat(val);
        else if (key == "diskOpac")    cfg->diskOpac = ParseFloat(val);
        else if (key == "dopplerMix")  cfg->dopplerMix = ParseFloat(val);
        else if (key == "diskBeam")    cfg->diskBeam = ParseFloat(val);
        else if (key == "diskContrast") cfg->diskContrast = ParseFloat(val);
        else if (key == "diskWind")    cfg->diskWind = ParseFloat(val);
        else if (key == "diskSpeed")   cfg->diskSpeed = ParseFloat(val);
        else if (key == "lensDepth")   cfg->lensDepth = ParseFloat(val);
        else if (key == "dilationMin") cfg->dilationMin = ParseFloat(val);
        else if (key == "timeDilation") cfg->timeDilation = ParseFloat(val, 1.0f);
        else if (key == "jetEnabled")   cfg->jetEnabled = ParseInt(val, 0) != 0;
        else if (key == "jetLength")    cfg->jetLength = ParseFloat(val, 20.0f);
        else if (key == "jetWidth")     cfg->jetWidth = ParseFloat(val, 0.15f);
        else if (key == "jetSpeed")     cfg->jetSpeed = ParseFloat(val, 2.0f);
        else if (key == "jetBrightness") cfg->jetBrightness = ParseFloat(val, 1.5f);
        else if (key == "particleEnabled")  cfg->particleEnabled = ParseInt(val, 0) != 0;
        else if (key == "particleCount")    cfg->particleCount = ParseInt(val, 2000);
        else if (key == "particleSize")     cfg->particleSize = ParseFloat(val, 2.0f);
        else if (key == "particleSpiral")   cfg->particleSpiral = ParseFloat(val, 1.0f);
        else if (key == "particleGravity")  cfg->particleGravity = ParseFloat(val, 3.0f);
        else if (key == "particleColorR")   cfg->particleColorR = ParseFloat(val, 1.0f);
        else if (key == "particleColorG")   cfg->particleColorG = ParseFloat(val, 0.4f);
        else if (key == "particleColorB")   cfg->particleColorB = ParseFloat(val, 0.05f);
        else if (key == "particleTrailLength") cfg->particleTrailLength = ParseInt(val, 40);
        else if (key == "particleCloudEnabled") cfg->particleCloudEnabled = ParseInt(val, 1) != 0;
        else if (key == "particleTiltX")       cfg->particleTiltX = ParseFloat(val, 1.5f);
        else if (key == "particleTiltY")       cfg->particleTiltY = ParseFloat(val, 0.0f);
        else if (key == "particleTiltZ")       cfg->particleTiltZ = ParseFloat(val, 0.0f);
        else if (key == "supermassiveMode") cfg->supermassiveMode = ParseInt(val, 0) != 0;
        else if (key == "starEnabled")        cfg->starEnabled      = ParseInt(val, 1) != 0;
        else if (key == "starMaxSize")        cfg->starMaxSize      = ParseFloat(val, 0.25f);
        else if (key == "starGrowthSpeed")    cfg->starGrowthSpeed  = ParseFloat(val, 0.06f);
        else if (key == "starFadeSpeed")      cfg->starFadeSpeed    = ParseFloat(val, 1.2f);
        else if (key == "starCoronaSize")     cfg->starCoronaSize   = ParseFloat(val, 2.0f);
        else if (key == "starCoronaBright")   cfg->starCoronaBright = ParseFloat(val, 0.7f);
        else if (key == "starFlareChance")    cfg->starFlareChance  = ParseFloat(val, 0.5f);
        else if (key == "starColorShift")     cfg->starColorShift   = ParseFloat(val, 0.15f);
        else if (key == "starWindEnabled")    cfg->starWindEnabled  = ParseInt(val, 1) != 0;
        else if (key == "starWindSpeed")      cfg->starWindSpeed    = ParseFloat(val, 1.0f);
        else if (key == "starWindCount")      cfg->starWindCount    = ParseInt(val, 1500);
        else if (key == "starWindBright")     cfg->starWindBright   = ParseFloat(val, 0.8f);
        else if (key == "starLensStrength")   cfg->starLensStrength   = ParseFloat(val, 0.3f);
        else if (key == "starInteractEnabled") cfg->starInteractEnabled = ParseInt(val, 1) != 0;
        else if (key == "starAbsorbRadius")    cfg->starAbsorbRadius    = ParseFloat(val, 0.25f);
        else if (key == "starAbsorbSpeed")     cfg->starAbsorbSpeed     = ParseFloat(val, 0.03f);
        else if (key == "streamBright")        cfg->streamBright        = ParseFloat(val, 2.0f);
        else if (key == "streamWidth")         cfg->streamWidth         = ParseFloat(val, 1.0f);
        else if (key == "streamSpiralTurns")   cfg->streamSpiralTurns   = ParseFloat(val, 2.0f);
        else if (key == "tidalEnabled")        cfg->tidalEnabled        = ParseInt(val, 1) != 0;
        else if (key == "tidalRadius")         cfg->tidalRadius         = ParseFloat(val, 0.30f);
        else if (key == "tidalRate")           cfg->tidalRate           = ParseFloat(val, 1.2f);
        else if (key == "tidalStretch")        cfg->tidalStretch        = ParseFloat(val, 1.0f);
        else if (key == "starEvolutionEnabled") cfg->starEvolutionEnabled = ParseInt(val, 1) != 0;
        else if (key == "evoExpandSpeed")       cfg->evoExpandSpeed       = ParseFloat(val, 0.25f);
        else if (key == "evoCollapseSpeed")     cfg->evoCollapseSpeed     = ParseFloat(val, 0.35f);
        else if (key == "supernovaBrightness")  cfg->supernovaBrightness  = ParseFloat(val, 2.5f);
        else if (key == "supernovaDuration")    cfg->supernovaDuration    = ParseFloat(val, 1.2f);
        else if (key == "collapseShake")        cfg->collapseShake        = ParseFloat(val, 0.8f);
        else if (key == "collapseVignette")     cfg->collapseVignette     = ParseFloat(val, 0.7f);
        else if (key == "collapseTension")       cfg->collapseTension       = ParseFloat(val, 0.6f);
        else if (key == "wdBrightness")         cfg->wdBrightness         = ParseFloat(val, 1.6f);
        else if (key == "wdCrystalDetail")      cfg->wdCrystalDetail      = ParseFloat(val, 7.0f);
        else if (key == "nsRotationSpeed")      cfg->nsRotationSpeed      = ParseFloat(val, 6.0f);
        else if (key == "nsJetLength")          cfg->nsJetLength          = ParseFloat(val, 0.9f);
        else if (key == "nsJetBrightness")      cfg->nsJetBrightness      = ParseFloat(val, 2.0f);
        else if (key == "nsAxisTilt")           cfg->nsAxisTilt           = ParseFloat(val, 0.6f);
        else if (key == "nsPulseRate")          cfg->nsPulseRate          = ParseFloat(val, 3.0f);
        else if (key == "miniBHRingBright")      cfg->miniBHRingBright      = ParseFloat(val, 1.5f);
        else if (key == "dualBHEnabled")         cfg->dualBHEnabled         = ParseInt(val, 1) != 0;
        else if (key == "miniBHMouseFollow")     cfg->miniBHMouseFollow     = ParseFloat(val, 8.0f);
        else if (key == "miniBHGravity")         cfg->miniBHGravity         = ParseFloat(val, 0.002f);
        else if (key == "miniBHDamping")         cfg->miniBHDamping         = ParseFloat(val, 2.0f);
        else if (key == "miniBHDiskIncl")       cfg->miniBHDiskIncl        = ParseFloat(val, 1.2f);
        else if (key == "miniBHLensStrength")    cfg->miniBHLensStrength    = ParseFloat(val, 1.0f);
        else if (key == "gwRippleStrength")      cfg->gwRippleStrength      = ParseFloat(val, 0.5f);
        else if (key == "accretionBridgeBright") cfg->accretionBridgeBright = ParseFloat(val, 1.0f);
        else if (key == "mergerEnabled")       cfg->mergerEnabled       = ParseInt(val, 1) != 0;
        else if (key == "mergerInspiralDur")   cfg->mergerInspiralDur   = ParseFloat(val, 12.0f);
        else if (key == "mergerMergerDur")     cfg->mergerMergerDur     = ParseFloat(val, 1.5f);
        else if (key == "mergerRingdownDur")   cfg->mergerRingdownDur   = ParseFloat(val, 6.0f);
        else if (key == "mergerInitialRadius") cfg->mergerInitialRadius = ParseFloat(val, 0.25f);
        else if (key == "mergerFlashBright")   cfg->mergerFlashBright   = ParseFloat(val, 2.0f);
        else if (key == "mergerGWBurst")       cfg->mergerGWBurst       = ParseFloat(val, 3.0f);
        else if (key == "iconDevourEnabled")    cfg->iconDevourEnabled    = ParseInt(val, 0) != 0;
        else if (key == "iconDevourRate")       cfg->iconDevourRate       = ParseFloat(val, 0.35f);
        else if (key == "iconDevourSpin")       cfg->iconDevourSpin       = ParseFloat(val, 1.2f);
        else if (key == "iconDevourStretch")    cfg->iconDevourStretch    = ParseFloat(val, 1.0f);
        else if (key == "iconDevourSwirl")      cfg->iconDevourSwirl      = ParseFloat(val, 1.0f);
        else if (key == "iconDevourStagger")    cfg->iconDevourStagger    = ParseFloat(val, 0.15f);
        else if (key == "iconDevourThrottleHz") cfg->iconDevourThrottleHz = ParseInt(val, 30);
        else if (key == "iconDevourRestore")    cfg->iconDevourRestore    = ParseInt(val, 1) != 0;
        else if (key == "displayMode") cfg->displayMode = ParseInt(val, 1);
        else if (key == "idleSeconds") cfg->idleSeconds = ParseInt(val, 300);
        else if (key == "playMode")    cfg->playMode = ParseInt(val, 0);
        else if (key == "slotSec")     cfg->slotSec = ParseFloat(val, 5.25f);
        else if (key == "fixedSize")   cfg->fixedSize = ParseInt(val, 0) != 0;
        else if (key == "fixedLevel")  cfg->fixedLevel = ParseFloat(val, 1.0f);
        else if (key == "demoMode")    cfg->demoMode = ParseInt(val, 1) != 0;
        else if (key == "presetCount") {
            cfg->presetCount = ParseInt(val, 0);
            if (cfg->presetCount > BlackholeConfig::MAX_PRESETS)
                cfg->presetCount = BlackholeConfig::MAX_PRESETS;
        }
        else if (key.substr(0, 7) == "preset_") {
            int idx = ParseInt(key.substr(7), -1);
            if (idx < 0 || idx >= BlackholeConfig::MAX_PRESETS) continue;
            // Parse: name,temp,incl,roll,inner,outer,opac,dopp,beam,gain,contrast,wind,speed,expo,star
            auto& p = cfg->presets[idx];
            size_t pos = 0;
            int field = 0;
            while (pos < val.size()) {
                auto comma = val.find(',', pos);
                std::string part = (comma == std::string::npos)
                    ? val.substr(pos) : val.substr(pos, comma - pos);
                switch (field) {
                    case 0:  strncpy(p.name, part.c_str(), 63); p.name[63] = 0; break;
                    case 1:  p.diskTemp = ParseFloat(part); break;
                    case 2:  p.diskIncl = ParseFloat(part); break;
                    case 3:  p.diskRoll = ParseFloat(part); break;
                    case 4:  p.diskInner = ParseFloat(part); break;
                    case 5:  p.diskOuter = ParseFloat(part); break;
                    case 6:  p.diskOpac = ParseFloat(part); break;
                    case 7:  p.dopplerMix = ParseFloat(part); break;
                    case 8:  p.diskBeam = ParseFloat(part); break;
                    case 9:  p.diskGain = ParseFloat(part); break;
                    case 10: p.diskContrast = ParseFloat(part); break;
                    case 11: p.diskWind = ParseFloat(part); break;
                    case 12: p.diskSpeed = ParseFloat(part); break;
                    case 13: p.exposure = ParseFloat(part); break;
                    case 14: p.starGain = ParseFloat(part); break;
                }
                field++;
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        }
    }

    // Sync smooth values
    cfg->smoothHoleRadius = cfg->holeRadius;
    cfg->smoothDiskGain   = cfg->diskGain;
    cfg->smoothDiskTemp   = cfg->diskTemp;
    cfg->smoothExposure   = cfg->exposure;
    cfg->smoothSpeed      = cfg->speed;
    cfg->smoothStarGain   = cfg->starGain;
    cfg->smoothDiskIncl   = cfg->diskIncl;
    cfg->smoothLensDepth  = cfg->lensDepth;
    cfg->smoothDilationMin = cfg->dilationMin;

    return true;
}

// ---- Apply preset to config ----
static void ApplyPreset(BlackholeConfig* cfg, const DiskPreset& p) {
    cfg->diskTemp     = p.diskTemp;
    cfg->diskIncl     = p.diskIncl;
    cfg->diskRoll     = p.diskRoll;
    cfg->diskInner    = p.diskInner;
    cfg->diskOuter    = p.diskOuter;
    cfg->diskOpac     = p.diskOpac;
    cfg->dopplerMix   = p.dopplerMix;
    cfg->diskBeam     = p.diskBeam;
    cfg->diskGain     = p.diskGain;
    cfg->diskContrast = p.diskContrast;
    cfg->diskWind     = p.diskWind;
    cfg->diskSpeed    = p.diskSpeed;
    cfg->exposure     = p.exposure;
    cfg->starGain     = p.starGain;
    printf("[BlackHole] Applied preset: temp=%.0f incl=%.2f gain=%.2f expo=%.2f\n",
           p.diskTemp, p.diskIncl, p.diskGain, p.exposure);
}

// ---- Dark sci-fi ImGui theme ----
static void SetupDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Deep space dark background
    ImVec4 bg         = ImVec4(0.02f, 0.02f, 0.06f, 0.94f);
    ImVec4 bgDark     = ImVec4(0.01f, 0.01f, 0.03f, 1.00f);
    ImVec4 bgMid      = ImVec4(0.04f, 0.04f, 0.10f, 1.00f);
    ImVec4 accent     = ImVec4(0.40f, 0.20f, 0.90f, 1.00f);  // Purple accent
    ImVec4 accentDim  = ImVec4(0.25f, 0.12f, 0.60f, 1.00f);
    ImVec4 accentLit  = ImVec4(0.55f, 0.30f, 1.00f, 1.00f);
    ImVec4 hotAccent  = ImVec4(0.80f, 0.30f, 0.10f, 1.00f);  // Orange for active
    ImVec4 text       = ImVec4(0.85f, 0.85f, 0.92f, 1.00f);
    ImVec4 textDim    = ImVec4(0.50f, 0.50f, 0.60f, 1.00f);

    colors[ImGuiCol_WindowBg]             = bg;
    colors[ImGuiCol_ChildBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]              = bgMid;
    colors[ImGuiCol_Border]               = accentDim;
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]              = bgDark;
    colors[ImGuiCol_FrameBgHovered]       = bgMid;
    colors[ImGuiCol_FrameBgActive]        = accentDim;
    colors[ImGuiCol_TitleBg]              = bgDark;
    colors[ImGuiCol_TitleBgActive]        = accentDim;
    colors[ImGuiCol_TitleBgCollapsed]     = bgDark;
    colors[ImGuiCol_MenuBarBg]            = bgDark;
    colors[ImGuiCol_ScrollbarBg]          = bgDark;
    colors[ImGuiCol_ScrollbarGrab]        = accentDim;
    colors[ImGuiCol_ScrollbarGrabHovered] = accent;
    colors[ImGuiCol_ScrollbarGrabActive]  = accentLit;
    colors[ImGuiCol_CheckMark]            = accentLit;
    colors[ImGuiCol_SliderGrab]           = accent;
    colors[ImGuiCol_SliderGrabActive]     = accentLit;
    colors[ImGuiCol_Button]               = accentDim;
    colors[ImGuiCol_ButtonHovered]        = accent;
    colors[ImGuiCol_ButtonActive]         = accentLit;
    colors[ImGuiCol_Header]               = accentDim;
    colors[ImGuiCol_HeaderHovered]        = accent;
    colors[ImGuiCol_HeaderActive]         = accentLit;
    colors[ImGuiCol_Separator]            = accentDim;
    colors[ImGuiCol_SeparatorHovered]     = accent;
    colors[ImGuiCol_ResizeGrip]           = accentDim;
    colors[ImGuiCol_ResizeGripHovered]    = accent;
    colors[ImGuiCol_ResizeGripActive]     = accentLit;
    colors[ImGuiCol_Tab]                  = bgDark;
    colors[ImGuiCol_TabHovered]           = accent;
    colors[ImGuiCol_TabSelected]          = accentDim;
    colors[ImGuiCol_TabDimmed]            = bgDark;
    colors[ImGuiCol_Text]                 = text;
    colors[ImGuiCol_TextDisabled]         = textDim;
    colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.40f, 0.20f, 0.90f, 0.35f);
    colors[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);

    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.GrabRounding      = 2.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding       = 4.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.WindowPadding     = ImVec2(12, 12);
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 6);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.GrabMinSize       = 8.0f;
    style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
}

// ---- GUI Init/Shutdown ----
void GUI_Init(Win32GL* wgl) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // Don't save imgui.ini

    SetupDarkTheme();

    // Load a Chinese-compatible font.
    ImGuiIO& ioFonts = ImGui::GetIO();
    ioFonts.Fonts->AddFontDefault();
    ImFontConfig fontCfg;
    fontCfg.MergeMode = true;
    fontCfg.GlyphRanges = ioFonts.Fonts->GetGlyphRangesChineseFull();
    static const ImWchar ranges[] = { 0x0020, 0x9FFF, 0xFF00, 0xFFEF, 0x3000, 0x303F, 0 };
    fontCfg.GlyphRanges = ranges;

    // Try common Windows Chinese fonts
    const char* fontPaths[] = {
        "C:/Windows/Fonts/msyh.ttc",   // Microsoft YaHei
        "C:/Windows/Fonts/simhei.ttf", // SimHei
        "C:/Windows/Fonts/simsun.ttc", // SimSun
        "C:/Windows/Fonts/msgothic.ttc"
    };
    bool loadedCJK = false;
    for (const char* fp : fontPaths) {
        if (GetFileAttributesA(fp) != INVALID_FILE_ATTRIBUTES) {
            if (ioFonts.Fonts->AddFontFromFileTTF(fp, 18.0f, &fontCfg, ranges)) {
                loadedCJK = true;
                break;
            }
        }
    }
    if (!loadedCJK) {
        ioFonts.Fonts->Build();
    }

    ImGui_ImplWin32_Init(wgl->hwnd);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void GUI_Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

bool GUI_IsActive() { return s_guiActive; }
void GUI_SetActive(bool active) { s_guiActive = active; }

// ---- Helper: slider with reset ----
static bool SliderWithReset(const char* label, float* v, float v_min, float v_max, float default_val, const char* format = "%.3f") {
    bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format);
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        *v = default_val;
        changed = true;
    }
    return changed;
}

// ---- Main GUI Render ----
void GUI_Render(BlackholeConfig* cfg) {
    if (!s_guiActive || !cfg) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(820, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

    ImGui::Begin(u8"\u9ED1\u6D1E\u63A7\u5236\u9762\u677F", &s_guiActive, ImGuiWindowFlags_MenuBar); // 黑洞控制面板

    // ---- Menu bar ----
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu(u8"\u6587\u4EF6")) { // 文件
            if (ImGui::MenuItem(u8"\u4FDD\u5B58\u914D\u7F6E")) { // 保存配置
                Config_Save(cfg, "blackhole_config.ini");
            }
            if (ImGui::MenuItem(u8"\u52A0\u8F7D\u914D\u7F6E")) { // 加载配置
                Config_Load(cfg, "blackhole_config.ini");
            }
            if (ImGui::MenuItem(u8"\u6062\u590D\u9ED8\u8BA4")) { // 恢复默认
                Config_SetDefaults(cfg);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(u8"\u89C6\u56FE")) { // 视图
            ImGui::MenuItem(u8"\u9AD8\u7EA7\u53C2\u6570", nullptr, &s_showAdvanced); // 高级参数
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Two-column layout: left = presets/modes, right = parameters
    ImGui::BeginChild("LeftPanel", ImVec2(260, 0), true);

    // ---- Built-in Presets ----
    ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u9ED1\u6D1E\u539F\u578B"); // 黑洞原型
    ImGui::Separator();

    for (int i = 0; i < s_builtInCount; i++) {
        bool selected = (s_selectedBuiltIn == i);
        if (ImGui::Selectable(s_builtInPresets[i].name, selected)) {
            s_selectedBuiltIn = i;
            s_selectedPreset = -1;
            ApplyPreset(cfg, s_builtInPresets[i]);
        }
        if (selected) {
            ImGui::SetItemDefaultFocus();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u81EA\u5B9A\u4E49\u9884\u8BBE"); // 自定义预设
    ImGui::Separator();

    // Custom preset list
    for (int i = 0; i < cfg->presetCount; i++) {
        bool selected = (s_selectedPreset == i);
        if (ImGui::Selectable(cfg->presets[i].name, selected)) {
            s_selectedPreset = i;
            s_selectedBuiltIn = -1;
            ApplyPreset(cfg, cfg->presets[i]);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Save new preset
    ImGui::Text(u8"\u4FDD\u5B58\u5F53\u524D\u53C2\u6570\u4E3A\u9884\u8BBE:"); // 保存当前参数为预设:
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##NewPresetName", s_newPresetName, sizeof(s_newPresetName));
    ImGui::PopItemWidth();
    if (ImGui::Button(u8"\u4FDD\u5B58\u9884\u8BBE") && s_newPresetName[0] != '\0') { // 保存预设
        if (cfg->presetCount < BlackholeConfig::MAX_PRESETS) {
            DiskPreset& p = cfg->presets[cfg->presetCount];
            strncpy(p.name, s_newPresetName, 63);
            p.name[63] = 0;
            p.diskTemp     = cfg->diskTemp;
            p.diskIncl     = cfg->diskIncl;
            p.diskRoll     = cfg->diskRoll;
            p.diskInner    = cfg->diskInner;
            p.diskOuter    = cfg->diskOuter;
            p.diskOpac     = cfg->diskOpac;
            p.dopplerMix   = cfg->dopplerMix;
            p.diskBeam     = cfg->diskBeam;
            p.diskGain     = cfg->diskGain;
            p.diskContrast = cfg->diskContrast;
            p.diskWind     = cfg->diskWind;
            p.diskSpeed    = cfg->diskSpeed;
            p.exposure     = cfg->exposure;
            p.starGain     = cfg->starGain;
            cfg->presetCount++;
            s_newPresetName[0] = '\0';
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ---- Playback Controls ----
    ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u64AD\u653E\u63A7\u5236"); // 播放控制
    ImGui::Checkbox(u8"\u6F14\u793A\u5DE1\u822A (\u81EA\u52A8\u9884\u8BBE\u5207\u6362)", &cfg->demoMode); // 演示巡航 (自动预设切换)
    if (cfg->demoMode) {
        const char* playModes[] = { u8"\u987A\u5E8F", u8"\u5FAA\u73AF", u8"\u968F\u673A" }; // 顺序, 循环, 随机
        ImGui::Combo(u8"\u64AD\u653E\u6A21\u5F0F", &cfg->playMode, playModes, 3); // 播放模式
        ImGui::SliderFloat(u8"\u5207\u6362\u95F4\u9694", &cfg->slotSec, 1.0f, 30.0f, u8"%.1f \u79D2"); // 切换间隔
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ---- Display Controls ----
    ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u663E\u793A\u63A7\u5236"); // 显示控制
    const char* displayModes[] = { u8"\u59CB\u7EC8\u663E\u793A", u8"\u7A7A\u95F2\u65F6" }; // 始终显示, 空闲时
    ImGui::Combo(u8"\u663E\u793A\u6A21\u5F0F", &cfg->displayMode, displayModes, 2); // 显示模式
    if (cfg->displayMode == 1) {
        ImGui::SliderInt(u8"\u7A7A\u95F2\u8D85\u65F6", &cfg->idleSeconds, 30, 1800, u8"%d \u79D2"); // 空闲超时
    }
    ImGui::Checkbox(u8"\u56FA\u5B9A\u5927\u5C0F", &cfg->fixedSize); // 固定大小
    if (cfg->fixedSize) {
        ImGui::SliderFloat(u8"\u5927\u5C0F\u7EA7\u522B", &cfg->fixedLevel, 0.1f, 1.0f); // 大小级别
    }

    ImGui::EndChild(); // LeftPanel

    ImGui::SameLine();

    // ---- Right panel: Parameters ----
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);

    ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u7269\u7406\u53C2\u6570"); // 物理参数
    ImGui::Separator();

    // ---- Kerr metric section ----
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), u8"Kerr \u5EA6\u89C4 (\u65CB\u8F6C\u9ED1\u6D1E)"); // Kerr 度规 (旋转黑洞)
    ImGui::Separator();
    ImGui::Checkbox(u8"\u542F\u7528 Kerr \u6A21\u5F0F", &cfg->kerrMode); // 启用 Kerr 模式
    if (cfg->kerrMode) {
        ImGui::SliderFloat(u8"\u81EA\u65CB\u53C2\u6570 a", &cfg->spinParam, 0.0f, 5.0f, u8"%.2f"); // 自旋参数 a (up to 5.0 for extreme Kerr / naked singularity)
        ImGui::SliderInt(u8"\u79EF\u5206\u6B65\u6570", &cfg->numSteps, 48, 256, u8"%d"); // 积分步数
        // Show Kerr-specific info
        float a = cfg->spinParam;
        float r_plus = 1.0f + sqrtf(std::max(1.0f - a * a, 0.0f));  // M=1
        float r_ergo_eq = 2.0f;  // at equator, always 2M
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.7f, 1.0f),
            u8"\u89C6\u754C\u534A\u5F84: %.2f M | \u80FD\u5C42\u539A\u5EA6(\u8D64\u9053): %.2f M", r_plus, r_ergo_eq - r_plus); // 视界半径 | 能层厚度(赤道)
    }

    ImGui::Spacing();

    // ---- Color gradient control ----
    ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.3f, 1.0f), u8"\u5438\u79EF\u76D8\u989C\u8272\u68AF\u5EA6"); // 吸积盘颜色梯度
    ImGui::Separator();
    ImGui::SliderFloat(u8"\u68AF\u5EA6\u5F3A\u5EA6", &cfg->colorGradient, 0.0f, 1.0f, u8"%.2f"); // 梯度强度
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.7f, 1.0f), u8"\u5185\u5708: \u84DD\u767D\u70ED | \u5916\u5708: \u6A59\u7EA2\u51B7"); // 内圈: 蓝白热 | 外圈: 橙红冷

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u65BD\u74E6\u8328\u67F4\u5C14\u5FB7\u53C2\u6570"); // 施瓦茨柴尔德参数
    ImGui::Separator();

    SliderWithReset(u8"\u9ED1\u6D1E\u534A\u5F84",   &cfg->holeRadius,  -1.0f, 0.15f,  -1.0f); // 黑洞半径
    SliderWithReset(u8"\u6F02\u79FB\u901F\u5EA6",    &cfg->speed,       -1.0f, 10.0f,  -1.0f); // 漂移速度
    SliderWithReset(u8"\u66DD\u5149",       &cfg->exposure,    -1.0f, 5.0f,   -1.0f); // 曝光
    SliderWithReset(u8"\u661F\u5149\u589E\u76CA",      &cfg->starGain,    -1.0f, 5.0f,   -1.0f); // 星光增益
    SliderWithReset(u8"\u5F15\u529B\u900F\u955C\u6DF1\u5EA6", &cfg->lensDepth,   -1.0f, 50.0f,  -1.0f); // 引力透镜深度
    SliderWithReset(u8"\u65F6\u95F4\u81A8\u80C0\u4E0B\u9650", &cfg->dilationMin, -1.0f, 1.0f,   -1.0f); // 时间膨胀下限

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u65F6\u95F4\u63A7\u5236"); // 时间控制
    ImGui::Separator();
    ImGui::SliderFloat(u8"\u65F6\u95F4\u6D41\u901F", &cfg->timeDilation, 0.1f, 5.0f, u8"%.2f x"); // 时间流速

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u76F8\u5BF9\u8BBA\u55B7\u6D41"); // 相对论喷流
    ImGui::Separator();
    ImGui::Checkbox(u8"\u542F\u7528\u55B7\u6D41", &cfg->jetEnabled); // 启用喷流
    if (cfg->jetEnabled) {
        ImGui::SliderFloat(u8"\u55B7\u6D41\u957F\u5EA6",   &cfg->jetLength,     5.0f, 50.0f); // 喷流长度
        ImGui::SliderFloat(u8"\u55B7\u6D41\u5BBD\u5EA6",   &cfg->jetWidth,      0.05f, 0.5f); // 喷流宽度
        ImGui::SliderFloat(u8"\u55B7\u6D41\u901F\u5EA6",   &cfg->jetSpeed,      0.5f, 10.0f); // 喷流速度
        ImGui::SliderFloat(u8"\u55B7\u6D41\u4EAE\u5EA6",   &cfg->jetBrightness, 0.1f, 5.0f); // 喷流亮度
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u7C92\u5B50\u7CFB\u7EDF"); // 粒子系统
    ImGui::Separator();
    ImGui::Checkbox(u8"\u542F\u7528\u7C92\u5B50", &cfg->particleEnabled); // 启用粒子
    if (cfg->particleEnabled) {
        ImGui::SliderInt(u8"\u7C92\u5B50\u6570\u91CF",   &cfg->particleCount, 100, 10000); // 粒子数量
        ImGui::SliderFloat(u8"\u7C92\u5B50\u5927\u5C0F",   &cfg->particleSize,  1.0f, 15.0f); // 粒子大小
        ImGui::SliderFloat(u8"\u87BA\u65CB\u901F\u5EA6",   &cfg->particleSpiral, 0.1f, 3.0f); // 螺旋速度
        ImGui::SliderFloat(u8"\u5438\u5F15\u5F3A\u5EA6",   &cfg->particleGravity, 0.1f, 5.0f); // 吸引强度
        ImGui::SliderFloat(u8"\u989C\u8272 R",   &cfg->particleColorR, 0.0f, 1.0f); // 颜色 R
        ImGui::SliderFloat(u8"\u989C\u8272 G",   &cfg->particleColorG, 0.0f, 1.0f); // 颜色 G
        ImGui::SliderFloat(u8"\u989C\u8272 B",   &cfg->particleColorB, 0.0f, 1.0f); // 颜色 B
        ImGui::SliderInt(u8"\u8F68\u8FF9\u957F\u5EA6", &cfg->particleTrailLength, 5, 60); // 轨迹长度
        ImGui::Checkbox(u8"\u7B49\u79BB\u5B50\u4E91", &cfg->particleCloudEnabled); // 等离子云
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.80f, 1.0f), u8"\u7C92\u5B50\u76D8\u503E\u659C"); // 粒子盘倾斜
        SliderWithReset(u8"\u524D\u540E\u503E\u659C", &cfg->particleTiltX, 0.0f, 3.14f, 1.5f, u8"%.2f rad"); // 前后倾斜
        SliderWithReset(u8"\u5DE6\u53F3\u503E\u659C", &cfg->particleTiltY, 0.0f, 3.14f, 0.0f, u8"%.2f rad"); // 左右倾斜
        SliderWithReset(u8"\u65CB\u8F6C\u89D2\u5EA6", &cfg->particleTiltZ, 0.0f, 6.28f, 0.0f, u8"%.2f rad"); // 旋转角度
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u8D85\u5927\u8D28\u91CF\u9ED1\u6D1E"); // 超大质量黑洞
    ImGui::Separator();
    if (ImGui::Checkbox(u8"\u542F\u7528\u8D85\u5927\u8D28\u91CF\u9ED1\u6D1E\u6A21\u5F0F", &cfg->supermassiveMode)) { // 启用超大质量黑洞模式
        // Apply immediate preset scale when toggled
    }
    if (cfg->supermassiveMode) {
        ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.80f, 1.0f),
            u8"\u8D85\u5927\u8D28\u91CF\u6A21\u5F0F\u4E0B\u5438\u79EF\u76D8\u503E\u89D2\u4E0E\u672C\u4F53\u540C\u6B65"); // 超大质量模式下吸积盘倾角与本体同步
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), u8"\u6052\u661F\u7CFB\u7EDF"); // 恒星系统
    ImGui::Separator();
    ImGui::Checkbox(u8"\u542F\u7528\u6052\u661F\u7CFB\u7EDF", &cfg->starEnabled); // 启用恒星系统
    if (cfg->starEnabled) {
        ImGui::SliderFloat(u8"\u6700\u5927\u534a\u5f84",   &cfg->starMaxSize,      0.02f, 0.5f, u8"%.3f"); // 最大半径
        ImGui::SliderFloat(u8"\u751F\u957F\u901F\u5EA6",   &cfg->starGrowthSpeed,  0.01f, 0.2f, u8"%.3f"); // 生长速度
        ImGui::SliderFloat(u8"\u6D88\u6563\u901F\u5EA6",   &cfg->starFadeSpeed,    0.3f, 3.0f, u8"%.2f"); // 消散速度
        ImGui::SliderFloat(u8"\u65E5\u5195\u5927\u5C0F",   &cfg->starCoronaSize,   1.2f, 4.0f, u8"%.1f x"); // 日冕大小
        ImGui::SliderFloat(u8"\u65E5\u5195\u4EAE\u5EA6",   &cfg->starCoronaBright, 0.1f, 2.0f, u8"%.2f"); // 日冕亮度
        ImGui::SliderFloat(u8"\u8000\u6591\u5F3A\u5EA6",   &cfg->starFlareChance,  0.0f, 1.0f, u8"%.2f"); // 耀斑强度
        ImGui::SliderFloat(u8"\u989C\u8272\u53D8\u5316\u901F\u7387", &cfg->starColorShift, 0.0f, 0.5f, u8"%.2f"); // 颜色变化速率

        ImGui::Spacing();
        ImGui::Checkbox(u8"\u592A\u9633\u98CE", &cfg->starWindEnabled); // 太阳风
        if (cfg->starWindEnabled) {
            ImGui::SliderFloat(u8"\u98CE\u901F", &cfg->starWindSpeed, 0.1f, 3.0f, u8"%.1f"); // 风速
            ImGui::SliderInt(u8"\u7C92\u5B50\u6570", &cfg->starWindCount, 100, 3000); // 粒子数
            ImGui::SliderFloat(u8"\u4EAE\u5EA6", &cfg->starWindBright, 0.1f, 2.0f, u8"%.2f"); // 亮度
        }

        ImGui::SliderFloat(u8"\u5F15\u529B\u626D\u66F2", &cfg->starLensStrength, 0.0f, 1.0f, u8"%.2f"); // 引力扭曲

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.1f, 1.0f), u8"\u9ED1\u6D1E-\u6052\u661F\u4EA4\u4E92"); // 黑洞-恒星交互
        ImGui::Separator();
        ImGui::Checkbox(u8"\u542F\u7528\u4EA4\u4E92", &cfg->starInteractEnabled); // 启用交互
        if (cfg->starInteractEnabled) {
            ImGui::SliderFloat(u8"\u5438\u6536\u534A\u5F84", &cfg->starAbsorbRadius, 0.05f, 0.6f, u8"%.2f"); // 吸收半径
            ImGui::SliderFloat(u8"\u5438\u6536\u901F\u5EA6", &cfg->starAbsorbSpeed, 0.005f, 0.1f, u8"%.3f"); // 吸收速度
            ImGui::SliderFloat(u8"\u80FD\u91CF\u6D41\u4EAE\u5EA6", &cfg->streamBright, 0.5f, 5.0f, u8"%.1f"); // 能量流亮度
            ImGui::SliderFloat(u8"\u80FD\u91CF\u6D41\u5BBD\u5EA6", &cfg->streamWidth, 0.2f, 3.0f, u8"%.1f"); // 能量流宽度
            ImGui::SliderFloat(u8"\u87BA\u65CB\u5708\u6570", &cfg->streamSpiralTurns, 0.5f, 4.0f, u8"%.1f"); // 螺旋圈数
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), u8"\u6F6E\u6C50\u6495\u88C2"); // 潮汐撕裂
        ImGui::Separator();
        ImGui::Checkbox(u8"\u542F\u7528\u6495\u88C2", &cfg->tidalEnabled); // 启用撕裂
        if (cfg->tidalEnabled) {
            ImGui::SliderFloat(u8"\u6F6E\u6C50\u534A\u5F84", &cfg->tidalRadius, 0.05f, 0.60f, u8"%.2f");  // 潮汐半径
            ImGui::SliderFloat(u8"\u5265\u79BB\u901F\u7387", &cfg->tidalRate, 0.1f, 5.0f, u8"%.2f");      // 剥离速率
            ImGui::SliderFloat(u8"\u5F62\u53D8\u5F3A\u5EA6", &cfg->tidalStretch, 0.0f, 4.0f, u8"%.2f");   // 形变强度
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1.0f), u8"\u6052\u661F\u6F14\u5316"); // 恒星演化
        ImGui::Separator();
        ImGui::Checkbox(u8"\u542F\u7528\u6F14\u5316(\u53F3\u952E)", &cfg->starEvolutionEnabled); // 启用演化(右键)
        if (cfg->starEvolutionEnabled) {
            ImGui::SliderFloat(u8"\u81A8\u80C0\u901F\u5EA6",   &cfg->evoExpandSpeed,    0.05f, 1.0f, u8"%.2f"); // 膨胀速度
            ImGui::SliderFloat(u8"\u574D\u7F29\u901F\u5EA6",   &cfg->evoCollapseSpeed,  0.05f, 1.5f, u8"%.2f"); // 坍缩速度
            ImGui::SliderFloat(u8"\u7206\u70B8\u4EAE\u5EA6",   &cfg->supernovaBrightness, 0.5f, 5.0f, u8"%.1f"); // 爆炸亮度
            ImGui::SliderFloat(u8"\u7206\u70B8\u6301\u7EED",   &cfg->supernovaDuration, 0.4f, 3.0f, u8"%.1fs"); // 爆炸持续
            ImGui::SliderFloat(u8"\u5C4F\u5E55\u6296\u52A8",   &cfg->collapseShake,     0.0f, 1.5f, u8"%.2f"); // 屏幕抖动
            ImGui::SliderFloat(u8"\u56DB\u5468\u53D8\u6697",   &cfg->collapseVignette,  0.0f, 1.0f, u8"%.2f"); // 四周变暗
            ImGui::SliderFloat(u8"\u5F20\u529B\u7EBF\u6761",   &cfg->collapseTension,    0.0f, 1.0f, u8"%.2f"); // 张力线条

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.85f, 0.85f, 1.0f, 1.0f), u8"\u767D\u77EE\u661F"); // 白矮星
            ImGui::SliderFloat(u8"\u4EAE\u5EA6", &cfg->wdBrightness, 0.5f, 3.0f, u8"%.2f"); // 亮度
            ImGui::SliderFloat(u8"\u6676\u4F53\u7EC6\u8282", &cfg->wdCrystalDetail, 2.0f, 10.0f, u8"%.1f"); // 晶体细节

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.5f, 1.0f, 1.0f), u8"\u4E2D\u5B50\u661F"); // 中子星
            ImGui::SliderFloat(u8"\u81EA\u8F6C\u901F\u5EA6", &cfg->nsRotationSpeed, 0.5f, 20.0f, u8"%.1f"); // 自转速度
            ImGui::SliderFloat(u8"\u55B7\u6D41\u957F\u5EA6", &cfg->nsJetLength, 0.1f, 2.0f, u8"%.2f"); // 喷流长度
            ImGui::SliderFloat(u8"\u55B7\u6D41\u4EAE\u5EA6", &cfg->nsJetBrightness, 0.5f, 5.0f, u8"%.2f"); // 喷流亮度
            ImGui::SliderFloat(u8"\u78C1\u8F74\u503E\u89D2", &cfg->nsAxisTilt, 0.0f, 1.57f, u8"%.2f"); // 磁轴倾角
            ImGui::SliderFloat(u8"\u8109\u51B2\u9891\u7387", &cfg->nsPulseRate, 0.5f, 10.0f, u8"%.1f"); // 脉冲频率

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.3f, 0.3f, 0.4f, 1.0f), u8"\u5C0F\u578B\u9ED1\u6D1E"); // 小型黑洞
            ImGui::SliderFloat(u8"\u5149\u5B50\u73AF\u4EAE\u5EA6", &cfg->miniBHRingBright, 0.5f, 4.0f, u8"%.2f"); // 光子环亮度
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.3f, 0.8f, 1.0f), u8"\u53CC\u9ED1\u6D1E\u7CFB\u7EDF"); // 双黑洞系统
        ImGui::Separator();
        ImGui::Checkbox(u8"\u542F\u7528\u53CC\u9ED1\u6D1E", &cfg->dualBHEnabled); // 启用双黑洞
        if (cfg->dualBHEnabled) {
            ImGui::SliderFloat(u8"\u9F20\u6807\u8DDF\u968F\u529B", &cfg->miniBHMouseFollow, 1.0f, 20.0f, u8"%.1f"); // 鼠标跟随力
            ImGui::SliderFloat(u8"\u5F15\u529B\u5F3A\u5EA6", &cfg->miniBHGravity, 0.0f, 0.01f, u8"%.4f"); // 引力强度
            ImGui::SliderFloat(u8"\u963B\u5C3C", &cfg->miniBHDamping, 0.5f, 5.0f, u8"%.1f"); // 阻尼
            ImGui::SliderFloat(u8"\u900F\u955C\u5F3A\u5EA6", &cfg->miniBHLensStrength, 0.0f, 6.0f, u8"%.2f"); // 透镜强度
            ImGui::SliderFloat(u8"\u5438\u79EF\u76D8\u503E\u89D2", &cfg->miniBHDiskIncl, 0.0f, 1.5f, u8"%.2f"); // 吸积盘倾角
            ImGui::SliderFloat(u8"\u5F15\u529B\u6CE2\u7EB9", &cfg->gwRippleStrength, 0.0f, 2.0f, u8"%.2f"); // 引力波纹
            ImGui::SliderFloat(u8"\u5438\u79EF\u6865\u4EAE\u5EA6", &cfg->accretionBridgeBright, 0.0f, 3.0f, u8"%.2f"); // 吸积桥亮度
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.2f, 1.0f), u8"\u9ED1\u6D1E\u5408\u5E76"); // 黑洞合并
        ImGui::Separator();
        ImGui::Checkbox(u8"\u542F\u7528\u5408\u5E76\u529F\u80FD", &cfg->mergerEnabled); // 启用合并功能
        if (cfg->mergerEnabled) {
            ImGui::SliderFloat(u8"\u65CB\u8FD1\u65F6\u95F4", &cfg->mergerInspiralDur, 3.0f, 30.0f, u8"%.1f \u79D2"); // 旋近时间
            ImGui::SliderFloat(u8"\u5E76\u5408\u65F6\u95F4", &cfg->mergerMergerDur, 0.5f, 5.0f, u8"%.1f \u79D2"); // 并合时间
            ImGui::SliderFloat(u8"\u94C3\u5C49\u65F6\u95F4", &cfg->mergerRingdownDur, 2.0f, 15.0f, u8"%.1f \u79D2"); // 铃宕时间
            ImGui::SliderFloat(u8"\u521D\u59CB\u8F68\u9053\u534A\u5F84", &cfg->mergerInitialRadius, 0.08f, 0.4f, u8"%.3f"); // 初始轨道半径
            ImGui::SliderFloat(u8"\u95EA\u5149\u4EAE\u5EA6", &cfg->mergerFlashBright, 0.0f, 5.0f, u8"%.2f"); // 闪光亮度
            ImGui::SliderFloat(u8"\u5F15\u529B\u6CE2\u7206\u53D1", &cfg->mergerGWBurst, 0.0f, 5.0f, u8"%.2f"); // 引力波爆发
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.7f, 1.0f),
                u8"\u63D0\u793A: \u5C06\u6052\u661F\u6F14\u5316\u4E3A\u5C0F\u9ED1\u6D1E\u540E, \u6309 M \u952E\u89E6\u53D1\u5408\u5E76"); // 提示: 将恒星演化为小黑洞后, 按 M 键触发合并
        }

        ImGui::TextColored(ImVec4(0.60f, 0.60f, 0.70f, 1.0f),
            u8"\u63D0\u793A: \u5DE6\u952E\u521B\u5EFA\u6052\u661F, \u53F3\u952E\u6F14\u5316(\u767D\u77EE\u661F->\u4E2D\u5B50\u661F), \u4E2D\u952E\u5761\u7F29\u6210\u9ED1\u6D1E"); // 提示: 左键创建恒星, 右键演化(白矮星->中子星), 中键坍缩成黑洞
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u5438\u79EF\u76D8"); // 吸积盘
    ImGui::Separator();

    SliderWithReset(u8"\u5438\u79EF\u76D8\u503E\u89D2", &cfg->diskIncl, 0.0f, 3.14f, 1.5f, u8"%.2f rad"); // 吸积盘倾角
    SliderWithReset(u8"\u76D8\u6E29\u5EA6",  &cfg->diskTemp,    -1.0f, 30000.0f, -1.0f, u8"%.0f K"); // 盘温度
    SliderWithReset(u8"\u76D8\u589E\u76CA",         &cfg->diskGain,    -1.0f, 10.0f,    -1.0f); // 盘增益

    if (s_showAdvanced) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.55f, 0.30f, 1.00f, 1.0f), u8"\u9AD8\u7EA7\u5438\u79EF\u76D8\u53C2\u6570"); // 高级吸积盘参数
        ImGui::Separator();

        SliderWithReset(u8"\u76D8\u6EDA\u52A8",        &cfg->diskRoll,    -1.0f, 6.28f,   -1.0f, u8"%.2f rad"); // 盘滚动
        SliderWithReset(u8"\u5185\u534A\u5F84",       &cfg->diskInner,   -1.0f, 10.0f,   -1.0f); // 内半径
        SliderWithReset(u8"\u5916\u534A\u5F84",       &cfg->diskOuter,   -1.0f, 20.0f,   -1.0f); // 外半径
        SliderWithReset(u8"\u76D8\u4E0D\u900F\u660E\u5EA6",     &cfg->diskOpac,    -1.0f, 1.0f,    -1.0f); // 盘不透明度
        SliderWithReset(u8"\u591A\u666E\u52D2\u6DF7\u5408",      &cfg->dopplerMix,  -1.0f, 1.0f,    -1.0f); // 多普勒混合
        SliderWithReset(u8"\u5149\u675F\u589E\u5F3A",        &cfg->diskBeam,    -1.0f, 10.0f,   -1.0f); // 光束增强
        SliderWithReset(u8"\u76D8\u5BF9\u6BD4\u5EA6",    &cfg->diskContrast, -1.0f, 5.0f,   -1.0f); // 盘对比度
        SliderWithReset(u8"\u76D8\u98CE\u901F",        &cfg->diskWind,    -1.0f, 20.0f,   -1.0f); // 盘风速
        SliderWithReset(u8"\u76D8\u901F\u5EA6",       &cfg->diskSpeed,   -1.0f, 20.0f,   -1.0f); // 盘速度
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ---- 桌面图标漩涡吞噬（Icon Devour）----
    if (ImGui::CollapsingHeader(u8"\u56FE\u6807\u541E\u566C (Icon Devour)")) { // 图标吞噬
        ImGui::Checkbox(u8"\u542F\u7528\u56FE\u6807\u541E\u566C", &cfg->iconDevourEnabled); // 启用图标吞噬
        ImGui::SliderFloat(u8"\u5185\u7F29\u901F\u7387", &cfg->iconDevourRate, 0.05f, 2.0f, u8"%.2f"); // 内缩速率
        ImGui::SliderFloat(u8"\u65CB\u8F6C\u89D2\u901F\u5EA6", &cfg->iconDevourSpin, 0.0f, 6.0f, u8"%.2f"); // 旋转角速度
        ImGui::SliderFloat(u8"\u9519\u5CF0\u95F4\u9694", &cfg->iconDevourStagger, 0.0f, 1.0f, u8"%.3f"); // 错峰间隔
        ImGui::SliderFloat(u8"\u62C9\u4F38\u589E\u76CA", &cfg->iconDevourStretch, 0.0f, 4.0f, u8"%.2f"); // 拉伸增益
        ImGui::SliderFloat(u8"\u6F26\u6DA1\u589E\u76CA", &cfg->iconDevourSwirl, 0.0f, 3.0f, u8"%.2f"); // 漩涡增益
        ImGui::SliderInt(u8"\u4E0B\u53D1\u9891\u7387", &cfg->iconDevourThrottleHz, 5, 60, u8"%d Hz"); // 下发频率
        ImGui::Checkbox(u8"\u505C\u6B62\u65F6\u8FD8\u539F", &cfg->iconDevourRestore); // 停止时还原
        if (ImGui::Button(u8"\u7ACB\u5373\u590D\u4F4D")) { // 立即复位
            DesktopIcons_Restore();
        }
        ImGui::TextWrapped(u8"\u72B6\u6001: %s", DesktopIcons_LastError()); // 状态
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Reset button
    if (ImGui::Button(u8"\u6062\u590D\u6240\u6709\u9ED8\u8BA4\u503C")) { // 恢复所有默认值
        Config_SetDefaults(cfg);
        s_selectedPreset = -1;
        s_selectedBuiltIn = -1;
    }

    ImGui::EndChild(); // RightPanel

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
