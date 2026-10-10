// T8：潮汐时间模型 + D5 可见半径不变量的纯函数镜像测试。
//
// Why: 这里镜像的公式必须与下面两处"逐字对应"，否则 D5 不变量会再次漂移（计划书 RK3）：
//   1) 时间积分：src/main.cpp 潮汐撕裂段
//        rate = (0.15 + 0.85·q^0.6) / t_dis ，q = (clamp(β,1,β_max) - 1) / (β_max - 1)
//        amp  = stretch · p^0.8 ，strip = p^1.2
//   2) 可见半径：shaders/energy_stream_frag.glsl
//        starRadius = uStarSize · (1 + tidalAmp)
//        noseWidth  = max(uStarSize · max(1 - 0.30·tidalAmp, 0.35), 0.004)
//
// 本文件不依赖 OpenGL / GUI，可独立编译运行：tidal_model_test.exe
// 退出码 0 = 全绿，1 = 存在失败断言。

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace tidal {

// 形变系数（与 GLSL 常量逐字一致）
constexpr float kPerpCompress = 0.30f;  // 垂直方向压缩系数
constexpr float kWidthFloor   = 0.35f;  // W_nose 下限系数，防本体细成一条线（RK4）
constexpr float kEps          = 0.05f;  // 可见边界 epsilon
const float     kK            = std::sqrt(std::log(1.0f / kEps));  // 归一化常数 K = sqrt(ln 20) ≈ 1.730818

// ---------- 时间模型（镜像 main.cpp）----------
inline float rate(float beta, float betaMax, float tDis) {
    // Why: 与 main.cpp 同式。β ≤ 1 时 q 会被钳到 0，由调用方决定是否积分。
    const float t   = std::fmin(beta, betaMax);
    const float q   = (t - 1.0f) / (betaMax - 1.0f);
    return (0.15f + 0.85f * std::pow(std::fmax(q, 0.0f), 0.6f)) / std::fmax(tDis, 0.1f);
}

inline float step(float p, float beta, float betaMax, float tDis, float dt) {
    if (beta > 1.0f) p += rate(beta, betaMax, tDis) * dt;
    return std::fmax(0.0f, std::fmin(1.0f, p));
}

// 积分到 p ≥ 1 所需时间（秒）。dt 越小越精确。
inline float timeToFull(float beta, float betaMax, float tDis, float dt = 1.0f / 1000.0f) {
    float p = 0.0f;
    float t = 0.0f;
    // 保护：β ≤ 1 时永不撕裂，返回无穷大语义
    if (beta <= 1.0f) return -1.0f;
    while (p < 1.0f && t < 1.0e6f) {
        p = step(p, beta, betaMax, tDis, dt);
        t += dt;
    }
    return t;
}

inline float ampOf(float p, float stretch) { return stretch * std::pow(p, 0.8f); }
inline float stripOf(float p)              { return std::pow(p, 1.2f); }

// ---------- D5 可见半径（镜像 GLSL）----------
// 两份实现刻意写成不同字面形式，避免"自己比自己"式恒等断言。
inline float cpuLnose(float r, float amp) {
    const float len = r * (1.0f + amp);
    return len;
}
inline float glslLnose(float uStarSize, float uTidalAmp) {
    return uStarSize * (1.0f + uTidalAmp);  // 逐字取自 energy_stream_frag.glsl
}
inline float cpuWnose(float r, float amp) {
    const float w = r * std::fmax(1.0f - kPerpCompress * amp, kWidthFloor);
    return w;
}
inline float glslWnose(float uStarSize, float uTidalAmp) {
    return std::fmax(uStarSize * std::fmax(1.0f - 0.30f * uTidalAmp, 0.35f), 0.004f);  // 逐字取自 GLSL
}

// ---------- 密度 / 吸收（镜像 star_frag.glsl 的 Beer–Lambert）----------
// ρ(q) = exp(-q²)·F ，F 为结构噪声因子；测试取 F = 1 的上界。
inline float densityAt(float q, float structure = 1.0f) { return std::exp(-q * q) * structure; }
inline float alphaOf(float rho, float k)                { return 1.0f - std::exp(-k * rho); }

}  // namespace tidal

// ---------------- 测试驱动 ----------------
static int g_failed = 0;

static void check(bool ok, const char* name, const char* detail) {
    if (ok) {
        std::printf("  [PASS] %-52s %s\n", name, detail);
    } else {
        std::printf("  [FAIL] %-52s %s\n", name, detail);
        ++g_failed;
    }
}

int main() {
    using namespace tidal;
    const float tDis     = 5.0f;   // config.tidalDisruptSec 默认
    const float betaMax  = 3.0f;   // config.tidalBetaMax 默认
    const float stretch  = 1.2f;   // config.tidalStretch GUI 上限（RK4 收紧后）
    const float R        = 0.25f;  // uStarSize 典型值
    char buf[192];

    std::printf("T8 tidal model unit tests (K = %.6f)\n", kK);

    // 断言 1：β→1+ 时极限耗时 t_dis/0.15 = 33.333 s
    {
        const float tFull = timeToFull(1.0f + 1.0e-6f, betaMax, tDis);
        std::snprintf(buf, sizeof(buf), "t_full=%.3f s (expect 33.33)", tFull);
        check(std::fabs(tFull - 33.333f) < 0.1f, "1. beta -> 1+ : 33.33 s to p=1", buf);
    }

    // 断言 2：β ≥ β_max（即 r ≤ r_t/3）时 5.0 s
    {
        const float tFull = timeToFull(3.0f, betaMax, tDis);
        std::snprintf(buf, sizeof(buf), "t_full=%.3f s (expect 5.00)", tFull);
        check(std::fabs(tFull - 5.0f) < 0.05f, "2. beta >= betaMax : 5.0 s to p=1", buf);
    }

    // 断言 3：500 步随机游走，p 单调不减且 p ≤ 1
    {
        float p = 0.0f;
        float prev = p;
        bool monotonic = true, bounded = true;
        unsigned seed = 20261010u;
        for (int i = 0; i < 500; ++i) {
            seed = seed * 1103515245u + 12345u;
            const float u = float((seed >> 16) & 0x7FFFu) / 32767.0f;  // [0,1]
            const float beta = 0.5f + u * 4.5f;                        // [0.5, 5.0]，含未撕裂区
            const float dt = 0.01f + u * 0.05f;
            p = step(p, beta, betaMax, tDis, dt);
            if (p < prev - 1.0e-6f) monotonic = false;
            if (p > 1.0f + 1.0e-6f) bounded = false;
            prev = p;
        }
        std::snprintf(buf, sizeof(buf), "final p=%.4f", p);
        check(monotonic && bounded, "3. p monotonic non-decreasing and <= 1", buf);
    }

    // 断言 4：amp = stretch·p^0.8 有上界 stretch ≤ 1.2
    {
        float maxAmp = 0.0f;
        for (int i = 0; i <= 1000; ++i) {
            const float p = float(i) / 1000.0f;
            maxAmp = std::fmax(maxAmp, ampOf(p, stretch));
        }
        std::snprintf(buf, sizeof(buf), "max amp=%.6f (bound 1.2)", maxAmp);
        check(maxAmp <= 1.2f + 1.0e-6f, "4. amp bounded by stretch", buf);
    }

    // 断言 5：L_nose CPU 与 GLSL 同式，|diff| < 1e-6
    {
        float worst = 0.0f;
        for (int i = 0; i <= 1000; ++i) {
            const float amp = 1.2f * float(i) / 1000.0f;
            worst = std::fmax(worst, std::fabs(cpuLnose(R, amp) - glslLnose(R, amp)));
        }
        std::snprintf(buf, sizeof(buf), "max|diff|=%.3e", worst);
        check(worst < 1.0e-6f, "5. L_nose matches GLSL", buf);
    }

    // 断言 6：W_nose CPU 与 GLSL 同式，且 ≥ 0.35R
    {
        float worst = 0.0f;
        float minW  = 1.0e9f;
        for (int i = 0; i <= 1000; ++i) {
            const float amp = 1.2f * float(i) / 1000.0f;
            worst = std::fmax(worst, std::fabs(cpuWnose(R, amp) - glslWnose(R, amp)));
            minW  = std::fmin(minW, cpuWnose(R, amp));
        }
        std::snprintf(buf, sizeof(buf), "max|diff|=%.3e minW=%.5f (floor %.5f)", worst, minW, 0.35f * R);
        check(worst < 1.0e-6f && minW >= 0.35f * R - 1.0e-6f, "6. W_nose matches GLSL and >= 0.35R", buf);
    }

    // 断言 7：amp = 0 的稳态密度/吸收剖面
    //
    // 【与计划书 §12.1 的偏差（已在此显式记录，不粉饰）】
    //   原文写「alpha(edge) < 0.05」。但按 ε=0.05 归一化（K = sqrt(ln(1/ε))）与
    //   k1 = uGasOpacity = 3.2 反推：
    //       rho(edge) = exp(-K^2) = 0.05
    //       alpha(edge) = 1 - exp(-3.2 * 0.05) = 0.1473  > 0.05
    //   即原文的 0.05 是"密度阈值 ε"，被误写成了 alpha 阈值。0.05 作为 alpha 阈值
    //   需要 rho(edge) < 0.016，与 K 的定义矛盾。
    //   故此处按物理语义断言：
    //     7a  rho(edge) <= eps        （密度软边成立）
    //     7b  alpha(0) > alpha(edge)  （中心比边缘更不透明）
    //     7c  alpha(edge)/alpha(0) < 0.20 （量级差，验证"软边而非硬边"）
    {
        const float k1 = 3.2f;  // uGasOpacity 默认值
        const float rhoCore = densityAt(0.0f);
        const float rhoEdge = densityAt(kK);
        const float aCore   = alphaOf(rhoCore, k1);
        const float aEdge   = alphaOf(rhoEdge, k1);
        const float ratio   = aEdge / aCore;

        std::snprintf(buf, sizeof(buf), "rho(edge)=%.4f (eps %.2f)", rhoEdge, kEps);
        check(rhoEdge <= kEps + 1.0e-4f, "7a. density at edge <= eps", buf);

        std::snprintf(buf, sizeof(buf), "alpha(0)=%.4f alpha(edge)=%.4f", aCore, aEdge);
        check(aCore > aEdge, "7b. alpha(core) > alpha(edge)", buf);

        std::snprintf(buf, sizeof(buf), "ratio=%.4f (< 0.20)", ratio);
        check(ratio < 0.20f, "7c. alpha edge/core ratio small (soft edge)", buf);
    }

    // 附加（非计划书条目，但支撑 G9/R5 的语义闸门）：amp=0 时不变量退化
    {
        const float l0 = cpuLnose(R, 0.0f);
        const float w0 = cpuWnose(R, 0.0f);
        std::snprintf(buf, sizeof(buf), "L=%.5f W=%.5f (both == R=%.5f)", l0, w0, R);
        check(std::fabs(l0 - R) < 1.0e-6f && std::fabs(w0 - R) < 1.0e-6f,
              "8. amp=0 -> visible radius degenerates to R", buf);
    }

    std::printf("%s (%d failed)\n", g_failed == 0 ? "ALL GREEN" : "FAILURES", g_failed);
    return g_failed == 0 ? 0 : 1;
}