// blackhole.glsl — a geodesic-traced black hole for Ghostty
//
// After Eric Bruneton's "Real-time High-Quality Rendering of Non-Rotating
// Black Holes" (https://ebruneton.github.io/black_hole_shader/). Bruneton
// precomputes Schwarzschild geodesics into lookup textures; a Ghostty custom
// shader is a single fragment pass with no custom textures, so here each
// pixel's null geodesic is integrated numerically instead — the Binet-form
// photon acceleration  a = -(3/2) h² x / r⁵  reproduces the exact
// Schwarzschild bending. Everything the camera sees falls out of that
// integration rather than being painted on:
//
//   * the shadow            — rays with impact parameter under
//                             b_crit = (3√3/2) r_s spiral into the horizon
//                             (your text really is gone, not faded)
//   * gravitational lensing — escaped rays are projected back onto the
//                             terminal "sky" plane: text bends, magnifies,
//                             and mirrors inside the Einstein ring
//   * photon ring           — rays winding near the r = 1.5 r_s photon sphere
//   * accretion disk        — a thin Keplerian disk the ray may cross several
//                             times (the far side arcs over and under the
//                             shadow); blackbody color from a Shakura–Sunyaev
//                             temperature profile, shifted and beamed by the
//                             relativistic factor g = √(1 − 1.5 r_s/r)/(1 − β·k̂)
//   * starfield             — a faint lensed sky so the bending reads even
//                             over empty terminal background
//
// Units: r_s (Schwarzschild radius) = 1. The screen mapping ties the shadow
// radius b_crit to HOLE_RADIUS * sz, so the size modes below keep working.

// ---------------------------------------------------------------- tunables --
// hole & lensing
const float HOLE_RADIUS   = 0.0200;
const float LENS_DEPTH    = 13.0000;
const float STAR_GAIN     = 0.0000;
// accretion disk geometry (radii in Schwarzschild radii)
const float DISK_INNER    = 1.8000;
const float DISK_OUTER    = 8.0000;
const float DISK_INCL     = 1.5000;
const float DISK_ROLL     = 0.3500;
// accretion disk matter & light
const float DISK_GAIN     = 2.2000;
const float DISK_OPACITY  = 0.9000;
const float DISK_TEMP     = 5500.0000;
const float DOPPLER_MIX   = 0.6000;
const float DISK_BEAM     = 2.5000;
const float DISK_SPEED    = 5.0000;
const float DISK_WIND     = 7.0000;
const float DISK_CONTRAST = 1.6000;
// light & screen
const float EXPOSURE      = 1.4000;
const float DRIFT_SPEED   = 1.0000;
const float WORK_AREA     = 0.3300;
const float DILATION_MIN  = 0.2000;
// token mode
const float TOKEN_AREA_MIN = 0.0100;
const float TOKEN_AREA_MAX = 0.5000;
const float TOKEN_HOME_X  = 0.9600;
const float TOKEN_HOME_Y  = 0.0400;
const float TOKEN_EASE    = 1.0000;
const float TOKEN_REACH   = 1.0000;
const float TOKEN_CALM    = 0.0400;
const float TOKEN_RUSH    = 1.1000;

// Runtime toggle for the desktop demo-tour preset cycling
// (uDemoMode declared in frag_desktop_header.glsl)

// geodesic integration steps per pixel
#define N_STEPS 48

// ---------------------------------------------------------------- Kerr metric --
// Simplified real-time Kerr implementation in Cartesian coordinates.
// Adds frame-dragging and quadrupole corrections to the Schwarzschild
// Binet-form acceleration  a = -(3/2) h² x / r⁵.
// Key structures (horizon, ergosphere, ISCO) computed analytically.

float kerr_event_horizon(float M, float a) {
    return M + sqrt(max(M * M - a * a, 0.0));
}

float kerr_ergosphere(float cos_theta, float M, float a) {
    return M + sqrt(max(M * M - a * a * cos_theta * cos_theta, 0.0));
}

float kerr_isco(float a, float prograde) {
    float z1 = 1.0 + pow(max(1.0 - a * a, 0.0), 1.0/3.0) *
               (pow(max(1.0 + a, 0.0), 1.0/3.0) + pow(max(1.0 - a, 0.0), 1.0/3.0));
    float z2 = sqrt(3.0 * a * a + z1 * z1);
    return 3.0 + z2 - prograde * sqrt(max((3.0 - z1) * (3.0 + z1 + 2.0 * z2), 0.0));
}

// Kerr-modified critical impact parameter (approximate)
// Prograde photons have smaller b_crit, retrograde have larger
float kerr_b_crit(float a) {
    return 2.5980762 * (1.0 - 0.35 * a);
}

// ---------------------------------------------------------------- size mode --
#define MODE_POMODORO 0
#define MODE_TOKENS   1
#define MODE_DEMO     2
#define SIZE_MODE MODE_DEMO

// Live state for MODE_TOKENS rides in on the *cursor color*
#define TOKEN_LEVEL -1

const ivec3 TOKEN_BASE_HI = ivec3(0xF, 0xB, 0x0);

float tokenFromBytes(ivec3 v) {
    ivec3 lo = v & 0xF;
    if ((v >> 4) != TOKEN_BASE_HI || lo.r != (lo.g ^ lo.b ^ 0x5)) return -1.0;
    int fill = (lo.g << 4) | lo.b;
    return fill > 250 ? -1.0 : float(fill) / 250.0;
}

float tokenDecode(vec3 cc) {
    vec3 c = clamp(cc, 0.0, 1.0);
    float lvl = tokenFromBytes(ivec3(floor(c * 255.0 + 0.5)));
    if (lvl >= 0.0) return lvl;
    vec3 s = mix(c * 12.92, 1.055 * pow(max(c, 1e-6), vec3(1.0 / 2.4)) - 0.055,
                 step(0.0031308, c));
    return tokenFromBytes(ivec3(floor(clamp(s, 0.0, 1.0) * 255.0 + 0.5)));
}

const float TOKEN_GLIDE_MIN  = 0.3000;
const float TOKEN_GLIDE_MAX  = 1.5000;
const float TOKEN_GLIDE_RATE = 10.0000;

float tokenLevel() {
    float cur = tokenDecode(iCurrentCursorColor.rgb);
    if (cur < 0.0) return -1.0;
    float prev = tokenDecode(iPreviousCursorColor.rgb);
    if (prev < 0.0) return cur;
    float T = clamp(abs(cur - prev) * TOKEN_GLIDE_RATE, TOKEN_GLIDE_MIN, TOKEN_GLIDE_MAX);
    return mix(prev, cur, smoothstep(0.0, T, iTime - iTimeCursorChange));
}

// ------------------------------------------------------------- demo mode --
const float DEMO_SEC      = 42.0000;
const float DEMO_GROW_SEC = 40.0000;
const float DEMO_XFADE    = 0.1800;

struct DiskLook {
    float temp, incl, roll, inner, outer, opac, dopp, beam,
          gain, contr, wind, speed, expo, star;
};
const DiskLook LOOK_DEFAULT = DiskLook(
    DISK_TEMP, DISK_INCL, DISK_ROLL, DISK_INNER, DISK_OUTER, DISK_OPACITY,
    DOPPLER_MIX, DISK_BEAM, DISK_GAIN, DISK_CONTRAST, DISK_WIND, DISK_SPEED,
    EXPOSURE, STAR_GAIN);
#define DEMO_N 8
const DiskLook DEMO_TOUR[DEMO_N] = DiskLook[DEMO_N](
    DiskLook( 5500.0, 1.50,  0.35, 1.8,  8.0, 0.90, 0.60, 2.5, 2.2, 1.6, 7.0, 5.0, 1.40, 0.0),
    DiskLook( 4500.0, 1.52,  0.10, 2.2,  7.0, 0.85, 0.35, 2.0, 1.4, 0.5, 7.0, 5.0, 1.20, 0.0),
    DiskLook( 3800.0, 0.55, -0.30, 2.2,  6.0, 0.45, 0.90, 3.5, 1.6, 0.4, 3.0, 2.5, 1.10, 0.0),
    DiskLook( 6500.0, 0.30,  0.00, 3.0, 10.0, 0.50, 0.80, 2.5, 1.0, 1.1, 7.0, 5.0, 1.00, 0.0),
    DiskLook(15000.0, 1.30,  0.35, 3.0, 14.0, 0.35, 1.00, 4.0, 1.2, 1.3, 8.0, 5.0, 0.80, 0.0),
    DiskLook(18000.0, 1.05,  0.55, 3.0, 16.0, 0.30, 1.00, 5.0, 1.0, 1.5, 9.0, 6.0, 0.75, 0.0),
    DiskLook( 5500.0, 1.50,  0.35, 1.8,  8.0, 0.00, 1.00, 2.5, 0.0, 1.6, 7.0, 5.0, 1.00, 0.6),
    DiskLook( 5500.0, 1.50,  0.35, 1.8,  8.0, 0.90, 0.60, 2.5, 2.2, 1.6, 7.0, 5.0, 1.40, 0.0));

DiskLook mixLook(DiskLook a, DiskLook b, float f) {
    return DiskLook(
        mix(a.temp,  b.temp,  f), mix(a.incl,  b.incl,  f),
        mix(a.roll,  b.roll,  f), mix(a.inner, b.inner, f),
        mix(a.outer, b.outer, f), mix(a.opac,  b.opac,  f),
        mix(a.dopp,  b.dopp,  f), mix(a.beam,  b.beam,  f),
        mix(a.gain,  b.gain,  f), mix(a.contr, b.contr, f),
        mix(a.wind,  b.wind,  f), mix(a.speed, b.speed, f),
        mix(a.expo,  b.expo,  f), mix(a.star,  b.star,  f));
}

DiskLook demoLook() {
    float u = mod(iTime, DEMO_SEC) / DEMO_SEC * float(DEMO_N);
    int   i = int(min(u, float(DEMO_N) - 0.001));
    float f = smoothstep(1.0 - DEMO_XFADE, 1.0, fract(u));
    return mixLook(DEMO_TOUR[i], DEMO_TOUR[(i + 1) % DEMO_N], f);
}

// ------------------------------------------------------ pomodoro --
const float WORK_PERIOD_MIN = 55.0000;
const float BREAK_MIN       = 5.0000;
const float IDLE_FADE_SEC   = 90.0000;
const float TIME_SCALE      = 1.0000;

#define B_CRIT 2.5980762

// ------------------------------------------------------------------- noise --
float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float vnoiseWrapY(vec2 p, float perY) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float y0 = mod(i.y, perY), y1 = mod(i.y + 1.0, perY);
    return mix(mix(hash21(vec2(i.x, y0)),       hash21(vec2(i.x + 1.0, y0)), f.x),
               mix(hash21(vec2(i.x, y1)),       hash21(vec2(i.x + 1.0, y1)), f.x),
               f.y);
}

vec2 mirrorUV(vec2 u) { return 1.0 - abs(1.0 - mod(u, 2.0)); }

vec2 rot(vec2 v, float a) {
    float c = cos(a), s = sin(a);
    return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

vec2 lissa(float t) {
    return vec2(0.75 * sin(t * 0.37) + 0.25 * sin(t * 0.83 + 1.0),
                0.70 * sin(t * 0.54 + 2.1) + 0.30 * sin(t * 1.07));
}

vec3 blackbody(float T) {
    float t = clamp(T, 1500.0, 40000.0) / 100.0;
    float r = t <= 66.0 ? 1.0
                        : clamp(1.292936 * pow(t - 60.0, -0.1332047), 0.0, 1.0);
    float g = t <= 66.0 ? clamp(0.3900816 * log(t) - 0.6318414, 0.0, 1.0)
                        : clamp(1.1298909 * pow(t - 60.0, -0.0755148), 0.0, 1.0);
    float b = t >= 66.0 ? 1.0
                        : (t <= 19.0 ? 0.0
                                     : clamp(0.5432068 * log(t - 10.0) - 1.1962540, 0.0, 1.0));
    return vec3(r, g, b);
}

vec3 stars(vec3 d) {
    vec2 sph = vec2(atan(d.x, -d.z), asin(clamp(d.y, -1.0, 1.0)));
    vec2 g   = sph * 40.0;
    vec2 id  = floor(g);
    float h  = hash21(id);
    if (h < 0.92) return vec3(0.0);
    vec2 f   = fract(g) - 0.5;
    vec2 off = (vec2(hash21(id + 17.3), hash21(id + 31.7)) - 0.5) * 0.7;
    float spark = smoothstep(0.10, 0.0, length(f - off));
    float tw    = 0.7 + 0.3 * sin(iTime * (0.5 + 2.0 * hash21(id + 5.1)) + 40.0 * h);
    vec3 tint   = mix(vec3(1.0, 0.82, 0.60), vec3(0.75, 0.85, 1.0), hash21(id + 2.9));
    return tint * spark * tw * ((h - 0.92) / 0.08);
}

// ------------------------------------------------------------------- image --
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2  res    = iResolution.xy;
    vec2  uv     = fragCoord / res;
    float aspect = res.x / res.y;

    // Star gravitational lensing: subtle UV distortion near stars
    float lensStr = uStarLensStr;
    if (lensStr > 0.0 && uStarPositions.z > 0.5) {
        vec2 lensOffset = vec2(0.0);
        // Star 0
        if (uStarPositions.z > 0.5) {
            vec2 d0 = uv - uStarPositions.xy;
            d0.x *= aspect;
            float r0 = length(d0);
            float s0 = max(uStarSizes.x, 0.001);
            if (r0 < s0 * 4.0 && r0 > s0 * 0.3) {
                float pull = lensStr * s0 * s0 / (r0 * r0 + s0 * s0);
                lensOffset += normalize(d0) * pull * 0.01;
            }
        }
        uv += lensOffset;
    }

    float yUp = 1.0 - uv.y;

    // Apply time dilation (0.1 = slow, 1.0 = normal, 5.0 = fast)
    float td = uTimeDilation > 0.0 ? uTimeDilation : 1.0;
    float t = iTime * DRIFT_SPEED * td;

    DiskLook L = LOOK_DEFAULT;
    if (SIZE_MODE == MODE_DEMO && uDemoMode > 0.5) L = demoLook();

    float rin  = max(L.inner, 1.6);
    float rout = max(L.outer, rin + 0.5);

    float I, sz;
    vec2  center;
    if (SIZE_MODE == MODE_POMODORO) {
        float workSec  = WORK_PERIOD_MIN * 60.0;
        float cycleSec = workSec + BREAK_MIN * 60.0;
        float wall     = iDate.w + iTime * (TIME_SCALE - 1.0);
        float phase    = mod(wall, cycleSec);
        float collapse = min(60.0, workSec * 0.15);
        float grow = clamp(phase / workSec, 0.0, 1.0)
                   * (1.0 - smoothstep(workSec - collapse, workSec, phase));
        I = mix(0.12, 1.0, grow);
        float idle = max(0.0, iTime - iTimeCursorChange);
        I *= 1.0 - smoothstep(IDLE_FADE_SEC, max(BREAK_MIN * 60.0, IDLE_FADE_SEC + 1.0), idle);
        sz = mix(0.22, 1.0, I);
        float ext = (rout / B_CRIT) * HOLE_RADIUS * sz;
        float yLo = WORK_AREA + 0.12 + ext;
        float yHi = max(yLo, 0.90 - ext);
        float spd = mix(0.35, 1.0, I);
        center = vec2(
            0.5 + (0.24 * sin(t * 0.21) + 0.05 * sin(t * 0.083)) * spd,
            1.0 - mix(yLo, yHi, 0.5 + (0.42 * sin(t * 0.157 + 2.0) + 0.08 * sin(t * 0.117)) * spd));
        center += I * vec2(0.040 * sin(t * 0.83) + 0.020 * sin(t * 1.31),
                           0.030 * sin(t * 1.03 + 1.0));
    } else {
        float live = tokenLevel();
        float lvl;
        if (SIZE_MODE == MODE_DEMO && uDemoMode > 0.5) {
            lvl = min(mod(iTime, DEMO_SEC) / DEMO_GROW_SEC, 1.0);
        } else if (uFixedSize != 0) {
            lvl = uFixedLevel;
        } else {
            lvl = 1.0;
        }
        if (lvl < 0.0) { fragColor = vec4(sampleBackground(uv), 1.0); return; }
        float g = pow(clamp(lvl, 0.0, 1.0), TOKEN_EASE);
        I = mix(0.10, 1.0, g);
        float rhMin = sqrt(TOKEN_AREA_MIN * aspect / 3.1415927);
        float rhMax = sqrt(TOKEN_AREA_MAX * aspect / 3.1415927);
        float rhT = mix(rhMin, rhMax, g) * (HOLE_RADIUS / 0.08);
        sz = rhT / max(HOLE_RADIUS, 1e-4);
        float marg = min(rhT * mix(1.45, 0.90, g), 0.5 * (1.0 - WORK_AREA - 0.03));
        float xPad = marg / aspect;
        vec2  fullLo = vec2(xPad, marg);
        vec2  fullHi = vec2(max(0.5, 1.0 - xPad),
                            max(marg, 1.0 - (WORK_AREA + 0.03 + marg)));
        vec2  corner = clamp(vec2(TOKEN_HOME_X, TOKEN_HOME_Y), fullLo, fullHi);
        float reach  = mix(0.06, max(TOKEN_REACH, 0.06), g);
        vec2  lo = vec2(mix(corner.x, fullLo.x, reach), fullLo.y);
        vec2  hi = vec2(fullHi.x, mix(corner.y, fullHi.y, reach));
        vec2  room   = max((hi - lo) * 0.5, vec2(0.0));
        vec2  wobAmp = min(vec2(0.010 + 0.030 * g), max(room * 0.35, vec2(0.006)));
        vec2  ampEff = max(room - wobAmp, vec2(0.0));
        vec2  wander = mix(lissa(t * TOKEN_CALM), lissa(t * TOKEN_RUSH), g);
        center = (lo + hi) * 0.5 + wander * ampEff
               + wobAmp * vec2(cos(t * 0.8), sin(t * 1.0));
    }
    float vis = smoothstep(0.0, 0.10, I);
    if (vis <= 0.0) {
        fragColor = vec4(sampleBackground(uv), 1.0);
        return;
    }
    float rh = HOLE_RADIUS * sz;

    // During merger, lock center to merger position (prevents wander/wobble)
    if (uMergerPhase > 0.5 && uMergerPhase < 4.5) {
        center = uMergerCenter;
    }

    // Apply merger offset to main BH center
    center += uMergerBHOffset;

    float dil = mix(1.0, DILATION_MIN, I);

    float shield = vis * smoothstep(WORK_AREA, WORK_AREA + 0.18, yUp);

    vec2  p = (uv - center) * vec2(aspect, 1.0);
    float plen = length(p);

    float W  = B_CRIT / max(rh, 1e-4);
    vec2  pr = rot(vec2(p.x, -p.y), L.roll) * W;
    float b  = length(pr);

    // Kerr modifications to critical impact parameter and capture radius
    float spin_a = uKerrMode > 0.5 ? clamp(uSpinParam, 0.0, 5.0) : 0.0;
    float b_crit_k = spin_a > 0.001 ? kerr_b_crit(spin_a) : B_CRIT;
    float r_horizon = kerr_event_horizon(0.5, spin_a);  // M=0.5 in r_s=1 units
    float capture_r2 = spin_a > 0.001 ? r_horizon * r_horizon : 1.0;

    // Kerr: adjust inner disk radius (ISCO depends on spin and direction)
    float rin_pro = spin_a > 0.001 ? kerr_isco(spin_a, 1.0) : rin;
    float rin_ret = spin_a > 0.001 ? kerr_isco(spin_a, -1.0) : rin;

    float window = exp(-pow(plen / (7.0 * rh), 2.0));

    float bmax = rout + 3.0;
    float Z0   = max(14.0, rout + 5.0);

    if (b >= bmax) {
        float u    = Z0 * inversesqrt(Z0 * Z0 + b * b);
        float defl = (2.0 / (W * W)) / max(plen, 1e-4)
                   * (1.29 * u + 0.07) * max(LENS_DEPTH - 2.14 * u + 0.75, 0.0)
                   * window * shield;
        vec2  dir  = p / max(plen, 1e-5);
        vec3  term;
        float ab = 0.035 * smoothstep(1.0, 2.0, b / bmax);
        for (int i = 0; i < 3; i++) {
            float k   = 1.0 + (float(i) - 1.0) * ab;
            vec2  sp  = p - dir * defl * k;
            vec2  suv = mirrorUV(center + sp / vec2(aspect, 1.0));
            term[i]   = sampleBackground(suv)[i];
        }
        vec3 d = normalize(vec3(-(pr / b) * (2.0 / b), -1.0));
        fragColor = vec4(term + stars(d) * L.star * window * shield, 1.0);
        return;
    }

    vec3  x  = vec3(pr, Z0);
    vec3  v  = vec3(0.0, 0.0, -1.0);
    float h2 = dot(pr, pr);

    float ci = cos(L.incl), si = sin(L.incl);
    // Disk normal: initially tilted around X axis, then rotated by roll (Z axis)
    // This allows arbitrary tilt direction
    float cr = cos(L.roll), sr = sin(L.roll);
    vec3 n = normalize(vec3(si * sr, si * cr, ci));  // full 3D tilt
    vec3 e2 = vec3(cr, -sr, 0.0);  // disk tangent direction (perpendicular to n in XY plane)
    float sdir = L.speed < 0.0 ? -1.0 : 1.0;
    float spd  = abs(L.speed);

    vec3  emitc = vec3(0.0);
    float trans = 1.0;
    bool  captured = false;
    float sPrev = dot(x, n);
    vec3  xPrev = x;
    float ergoEmit = 0.0;  // ergosphere glow accumulator

    // Kerr spin axis: perpendicular to accretion disk (= disk normal)
    vec3 spin_axis = normalize(n);

    int numSteps = uNumSteps > 0 ? uNumSteps : N_STEPS;
    for (int i = 0; i < 256; i++) {  // upper bound
        if (i >= numSteps) break;
        float r2 = dot(x, x);
        if (r2 < capture_r2) { captured = true; break; }
        if (x.z < -Z0 && v.z < 0.0) break;
        if (r2 > 4.0 * Z0 * Z0) break;
        float r  = sqrt(r2);

        // Adaptive step size: smaller near horizon for stability
        float r_safe = max(r - r_horizon, 0.1);
        float dt_step = clamp(0.10 * r_safe, 0.015, 0.6);

        // Schwarzschild base acceleration
        vec3 a = -1.5 * h2 * x / (r2 * r2 * r);

        // Kerr frame-dragging correction
        if (spin_a > 0.001) {
            vec3 r_hat = x / r;
            float cos_th = abs(dot(r_hat, spin_axis));

            // Frame-dragging: tangential acceleration around spin axis
            // Direction: cross(r_hat, spin_axis) gives azimuthal direction
            vec3 phi_hat = normalize(cross(spin_axis, r_hat) + 1e-6);
            float drag_strength = 0.3 * spin_a * h2 / (r2 * r * r_safe);
            a += drag_strength * phi_hat;

            // Quadrupole correction (spin-spin interaction)
            float spin_proj = dot(r_hat, spin_axis);
            a -= spin_a * spin_a * 0.1 * h2 * (3.0 * spin_proj * spin_proj - 1.0) * r_hat / (r2 * r * r_safe);

            // Clamp total acceleration magnitude to prevent divergence
            float a_mag = length(a);
            float a_max = 2.0 * h2 / (r_safe * r_safe * r_safe);
            if (a_mag > a_max) a *= a_max / a_mag;

            // Ergosphere glow
            float r_ergo = kerr_ergosphere(cos_th, 0.5, spin_a);
            if (r < r_ergo && r > r_horizon) {
                float ergo_frac = clamp(1.0 - (r - r_horizon) / max(r_ergo - r_horizon, 0.01), 0.0, 1.0);
                ergoEmit += ergo_frac * 0.003 * trans;
            }
        }

        // Leapfrog integration
        v += a * (0.5 * dt_step);
        x += v * dt_step;
        r2 = dot(x, x);
        r  = sqrt(r2);

        // Recompute acceleration at new position
        a = -1.5 * h2 * x / (r2 * r2 * r);
        if (spin_a > 0.001) {
            float r_safe2 = max(sqrt(max(r2, capture_r2)) - r_horizon, 0.1);
            vec3 r_hat2 = x / max(sqrt(max(r2, capture_r2)), 0.01);
            vec3 phi_hat2 = normalize(cross(spin_axis, r_hat2) + 1e-6);
            float drag_strength2 = 0.3 * spin_a * h2 / (max(r2, capture_r2) * max(sqrt(max(r2, capture_r2)), 0.01) * r_safe2);
            a += drag_strength2 * phi_hat2;

            float spin_proj2 = dot(r_hat2, spin_axis);
            a -= spin_a * spin_a * 0.1 * h2 * (3.0 * spin_proj2 * spin_proj2 - 1.0) * r_hat2 / (max(r2, capture_r2) * max(sqrt(max(r2, capture_r2)), 0.01) * r_safe2);

            float a_mag2 = length(a);
            float a_max2 = 2.0 * h2 / (r_safe2 * r_safe2 * r_safe2);
            if (a_mag2 > a_max2) a *= a_max2 / a_mag2;
        }
        v += a * (0.5 * dt_step);

        float s = dot(x, n);
        if (s * sPrev < 0.0 && trans > 0.02) {
            float tc = sPrev / (sPrev - s);
            vec3  xc = mix(xPrev, x, tc);
            float rc = length(xc);

            // Kerr: use direction-dependent ISCO for inner radius
            float rin_eff = rin;
            if (spin_a > 0.001) {
                vec3 gas_dir_kerr = normalize(cross(n, xc)) * sdir;
                float prograde_sign = dot(gas_dir_kerr, spin_axis);
                rin_eff = mix(rin_ret, rin_pro, smoothstep(-0.3, 0.3, prograde_sign));
            }

            if (rc > rin_eff && rc < rout) {
                float band = smoothstep(rin_eff, rin_eff * 1.25, rc)
                           * (1.0 - smoothstep(rout * 0.70, rout, rc));

                float phi   = atan(dot(xc, e2), xc.x);
                float turns = phi / 6.2831853;
                float kep   = pow(rin_eff / rc, 1.5) * 1.5;
                float gloc  = sqrt(max(1.0 - 1.5 / rc, 0.02));
                float swirl = rc * L.wind * 0.12 - t * kep * spd * gloc * dil * sdir;
                float streaks = vnoiseWrapY(vec2(rc * 2.8, turns * 19.0 + swirl * 3.0), 19.0) * 0.65 +
                                vnoiseWrapY(vec2(rc * 1.0, turns * 9.0  + swirl * 1.5 + 7.0), 9.0) * 0.35;
                streaks = 0.35 + L.contr * streaks * streaks;

                vec3  gasdir = normalize(cross(n, xc)) * sdir;

                // Kerr: enhanced Doppler beaming due to frame dragging
                float beta = clamp(inversesqrt(max(2.0 * (rc - 1.0), 0.2)), 0.0, 0.99);
                float g = gloc / max(1.0 + beta * dot(gasdir, normalize(v)), 0.05);

                // Frame dragging adds extra velocity to prograde side
                if (spin_a > 0.001) {
                    float fd_boost = spin_a * 1.5 / (rc * rc);
                    float prograde = dot(gasdir, spin_axis) * sdir;
                    g *= 1.0 + fd_boost * prograde;
                }
                g = mix(1.0, g, L.dopp);

                float xpr   = max(1.0 - sqrt(rin_eff / rc), 0.0);
                float tprof = pow(rin_eff / rc, 0.75) * pow(xpr, 0.25) / 0.488;
                vec3  cbb   = blackbody(L.temp * tprof * g);

                // Color gradient: inner = blue-white hot, outer = red-orange cooler
                float gradient = uColorGradient;
                if (gradient > 0.01) {
                    float rNorm = clamp((rc - rin_eff) / (rout - rin_eff), 0.0, 1.0);
                    // Inner: hot blue-white, Outer: cooler orange-red
                    vec3 innerCol = vec3(0.7, 0.8, 1.0);  // hot blue-white
                    vec3 outerCol = vec3(1.0, 0.5, 0.2);  // cooler orange-red
                    vec3 gradientCol = mix(innerCol, outerCol, rNorm);
                    cbb = mix(cbb, gradientCol * cbb * 1.5, gradient * 0.6);
                }

                float boost = pow(g, L.beam);

                float density = band * streaks;
                emitc += trans * cbb * (L.gain * 2.2 * density * tprof * tprof * boost);
                trans *= 1.0 - clamp(L.opac * density, 0.0, 1.0);
            }
        }
        sPrev = s;
        xPrev = x;
    }
    if (!captured && dot(x, x) < capture_r2 * 4.0) captured = true;

    vec3 bg = vec3(0.0);
    if (!captured) {
        vec3 d = normalize(v);
        bg += stars(d) * L.star * window * shield;
        if (d.z < -0.05) {
            float tpl = (-LENS_DEPTH - x.z) / d.z;
            vec3  hp  = x + d * tpl;
            vec2  q   = rot(hp.xy, -L.roll) / W;
            vec2  sp  = vec2(q.x, -q.y);
            vec2  suv = mirrorUV(center + (p + (sp - p) * window * shield) / vec2(aspect, 1.0));
            float toward = smoothstep(0.05, 0.35, -d.z);
            bg += sampleBackground(suv) * toward;
        }
    }

    vec3 col = bg * trans + (vec3(1.0) - exp(-emitc * L.expo));

    // Kerr: add ergosphere glow
    if (spin_a > 0.001 && ergoEmit > 0.001) {
        float ergo_clamped = min(ergoEmit, 0.3);  // prevent overflow
        vec3 ergo_color = mix(vec3(0.1, 0.05, 0.2), vec3(0.3, 0.2, 0.8), ergo_clamped);
        col += ergo_color * ergo_clamped * L.expo * window * shield;
    }

    // ---- Relativistic jets (enhanced: multi-layer, helical, multi-frequency) ----
    if (uJetEnabled > 0.5) {
        float jetLen   = uJetLength > 0.0 ? uJetLength : 20.0;
        float jetW     = uJetWidth  > 0.0 ? uJetWidth  : 0.15;
        float jetSpd   = uJetSpeed  > 0.0 ? uJetSpeed  : 2.0;
        float jetBrt   = uJetBrightness > 0.0 ? uJetBrightness : 1.5;

        // Jet axis: disk normal direction (rotated by L.roll)
        vec2 jetDir2 = vec2(sin(L.roll), -cos(L.roll));
        vec2 jetPerp = vec2(jetDir2.y, -jetDir2.x);

        // Kerr: jet tilt due to spin precession
        if (spin_a > 0.001) {
            float tiltAngle = spin_a * 0.15;
            float tiltX = sin(tiltAngle) * cos(t * 0.2);
            float tiltY = sin(tiltAngle) * sin(t * 0.2);
            jetDir2 = normalize(jetDir2 + vec2(tiltX, tiltY) * 0.1);
            jetPerp = vec2(jetDir2.y, -jetDir2.x);
        }

        vec2 toPixel = p;

        for (int sign = -1; sign <= 1; sign += 2) {
            vec2  jDir   = jetDir2 * float(sign);
            float along  = dot(toPixel, jDir);
            float perp   = length(toPixel - jDir * along);
            // Signed perpendicular for helical twist
            float perpSigned = dot(toPixel - jDir * along, jetPerp);

            if (along > 0.0) {
                float rJet = along / max(rh, 1e-4);

                // === LAYER 1: Wide outer sheath (radio lobe) ===
                float sheathW = jetW * (1.5 + rJet * 0.08);
                float sheathFade = (1.0 - smoothstep(jetLen * 0.3, jetLen, rJet));
                float sheath = exp(-perp * perp / (2.0 * sheathW * sheathW * rh * rh)) * sheathFade;
                // Slow flow in sheath
                float sheathFlow = 0.5 + 0.5 * sin(rJet * 0.4 - t * jetSpd * 0.3);
                sheath *= (0.3 + 0.7 * sheathFlow);
                // Radio color: warm orange-red
                vec3 sheathCol = mix(vec3(0.6, 0.3, 0.1), vec3(0.8, 0.5, 0.2), sheathFlow);

                // === LAYER 2: Core jet beam ===
                float coreW = jetW * (0.4 + rJet * 0.03);
                float distFade = (1.0 - smoothstep(jetLen * 0.5, jetLen, rJet));
                float core = exp(-perp * perp / (2.0 * coreW * coreW * rh * rh)) * distFade;
                // Multi-frequency flow knots
                float flow1 = 0.5 + 0.5 * sin(rJet * 0.8 - t * jetSpd);
                float flow2 = 0.5 + 0.5 * sin(rJet * 1.7 - t * jetSpd * 0.7 + 1.5);
                float flow3 = 0.5 + 0.5 * sin(rJet * 3.1 - t * jetSpd * 1.3 + 3.0);
                float streak = mix(mix(flow1, flow2, 0.4), flow3, 0.25);
                // Bright base, fading outward
                float baseBright = smoothstep(0.0, 2.0, rJet);
                core *= (0.4 + 0.6 * streak) * baseBright;
                // X-ray / gamma color: blue-white core
                vec3 gammaCol = vec3(0.4, 0.5, 1.0);
                vec3 xrayCol  = vec3(0.6, 0.75, 1.0);
                vec3 synchCol = vec3(0.85, 0.92, 1.0);
                vec3 coreCol = mix(gammaCol, xrayCol, streak * streak);
                coreCol = mix(coreCol, synchCol, core * core * 0.3);

                // === LAYER 3: Helical magnetic field lines ===
                // Kerr: enhanced spiral twist
                float spiralRate = spin_a > 0.001 ? 2.5 + spin_a * 3.0 : 2.5;
                float helixAngle = perpSigned / max(perp, 1e-5);
                float helixPhase = rJet * spiralRate - t * jetSpd * 0.6 + atan(helixAngle) * 2.0;
                float helix = 0.5 + 0.5 * sin(helixPhase);
                // Helix visible only in the core region
                float helixW = coreW * 1.8;
                float helixEnv = exp(-perp * perp / (2.0 * helixW * helixW * rh * rh)) * distFade;
                helix = helix * helix * helixEnv * smoothstep(1.0, 3.0, rJet);
                // Synchrotron blue-violet helix
                vec3 helixCol = vec3(0.5, 0.4, 1.0);

                // === LAYER 4: Internal knot structure ===
                // Bright knots at quasi-regular intervals (like M87 jet)
                float knotPhase = fract(rJet * 0.15 - t * jetSpd * 0.02);
                float knot = exp(-pow((knotPhase - 0.5) / 0.08, 2.0));
                knot *= core * 0.8 * smoothstep(3.0, 6.0, rJet);
                vec3 knotCol = vec3(0.9, 0.95, 1.0);

                // === Composite ===
                vec3 jetTotal = sheathCol * sheath * 0.35
                              + coreCol * core * 1.0
                              + helixCol * helix * 0.5
                              + knotCol * knot * 0.6;
                col += jetTotal * jetBrt * window * shield * 0.3;
            }
        }
    }

    fragColor = vec4(col, 1.0);
}

// ---- Entry point for desktop/preview mode ----
void main() {
    // Screen shake during stellar collapse: offset sampling coord by shared uShakeOffset
    vec2 fc = gl_FragCoord.xy + uShakeOffset;
    mainImage(fragColor, fc);

    // Vignette: darken screen edges during collapse
    vec2 q = (gl_FragCoord.xy / iResolution.xy) - 0.5;
    float vig = clamp(1.0 - dot(q, q) * 3.5 * uCollapseVignette, 0.0, 1.0);
    fragColor.rgb *= vig;

    // Fine tension lines: thin radial bright streaks from screen center
    if (uCollapseTension > 0.001) {
        float ang = atan(q.y, q.x);
        float tline = pow(0.5 + 0.5 * cos(ang * 24.0), 3.0);
        // distance-based fade so lines are longer near center collapse
        float distFade = clamp(1.0 - length(q) * 1.2, 0.0, 1.0);
        fragColor.rgb += vec3(0.55, 0.65, 1.0) * tline * uCollapseTension * 0.18 * (0.4 + 0.6 * distFade);
    }

    // ===== Black hole merger effects =====
    // Inspiral: center brightness grows as BHs approach + GW ripple
    if (uMergerPhase > 0.5 && uMergerPhase < 1.5) {
        float prog = uMergerProgress;
        // Bright glow at center that intensifies dramatically
        float centerGlow = pow(prog, 2.0) * 3.0;
        float radial = clamp(1.0 - length(q) * 3.0, 0.0, 1.0);
        fragColor.rgb += vec3(1.0, 0.9, 0.7) * centerGlow * radial * radial;

        // GW ripple: frequency increases with progress
        float amp = prog * 0.012;
        float freq = uMergerGWFreq;
        float r = length(q);
        float wave = sin(r * freq * 5.0 - iTime * freq) * amp;
        fragColor.rgb += vec3(0.1, 0.2, 0.4) * abs(wave) * 20.0 * prog;
    }

    // Merger flash: violent explosive burst
    if (uMergerFlash > 0.001 && uMergerPhase > 1.5 && uMergerPhase < 2.5) {
        float flash = uMergerFlash;
        // Strong radial burst from center
        float radial = clamp(1.0 - length(q) * 1.5, 0.0, 1.0);
        // Explosive white-out at peak
        fragColor.rgb = mix(fragColor.rgb, vec3(1.0, 0.95, 0.85), flash * (0.4 + 0.6 * radial));
        // Add shockwave ring
        float shockR = uMergerProgress * 0.8;
        float shockW = 0.05;
        float shock = exp(-pow((length(q) - shockR) / shockW, 2.0)) * flash;
        fragColor.rgb += vec3(0.8, 0.9, 1.0) * shock * 0.8;
    }

    // Ringdown: decaying gravitational wave distortion
    if (uMergerRingdownAmp > 0.001 && uMergerPhase > 2.5) {
        float amp = abs(uMergerRingdownAmp) * 0.015;
        float freq = uMergerGWFreq;
        float r = length(q);
        float wave = sin(r * freq * 3.0 - iTime * freq) * amp;
        fragColor.rgb += vec3(0.15, 0.25, 0.5) * abs(wave) * 20.0;
        // Fading afterglow at center
        float afterglow = abs(uMergerRingdownAmp) * 0.5;
        float radial = clamp(1.0 - length(q) * 3.0, 0.0, 1.0);
        fragColor.rgb += vec3(0.6, 0.7, 1.0) * afterglow * radial * radial * 0.3;
    }
}
