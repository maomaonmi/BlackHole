# Kerr 度规黑洞 - 独立实验模块

## 概述

本模块是对 BlackHole 项目中 Kerr 度规旋转黑洞的独立实现。与现有施瓦茨柴尔德（非旋转）黑洞分离，便于独立开发和测试，不影响现有稳定版本。

## 核心特性

- **帧拖曳效应**：旋转黑洞拖拽周围时空，产生不对称的光线弯曲
- **能层可视化**：渲染事件视界与静态极限之间的能层区域
- **螺旋喷流**：喷流沿自转轴呈螺旋状上升
- **连续过渡**：自旋参数 a 从 0 到 0.99 连续变化，a=0 退化为施瓦茨柴尔德

## 文件结构

```
kerr/
├── shaders/
│   ├── kerr_blackhole.glsl      # Kerr 主渲染 shader（完整独立）
│   ├── kerr_geodesic.glsl       # 测地线方程库（可复用）
│   └── kerr_ergosphere.glsl     # 能层渲染函数
├── src/
│   ├── kerr_config.h            # Kerr 配置结构体
│   └── kerr_config.cpp          # ImGui 面板 + Uniform 绑定
├── docs/
│   ├── 计划书.md                # 详细实现计划
│   ├── kerr_math.md             # 数学推导
│   └── kerr_vs_schwarzschild.md # 视觉对比文档
└── README.md                    # 本文件
```

## 快速开始

### 1. 编译

将 `kerr_blackhole.glsl` 作为主 shader 加载（替换 `blackhole.glsl`）：

```cpp
// 在 main.cpp 中
std::string fragBody = ReadFile("kerr/shaders/kerr_blackhole.glsl");
```

### 2. 配置

在 ImGui 面板中开启 Kerr 模式：

```cpp
#include "kerr_config.h"
KerrConfig kerrCfg;
// 在渲染循环中
RenderKerrConfig(kerrCfg);
BindKerrUniforms(program, kerrCfg);
```

### 3. 参数说明

| 参数 | 范围 | 默认 | 说明 |
|------|------|------|------|
| `spinParam` | 0.0~0.99 | 0.5 | 黑洞自旋参数 |
| `kerrMode` | 0/1 | 0 | 0=施瓦茨柴尔德, 1=Kerr |
| `showErgosphere` | bool | true | 显示能层 |
| `ergosphereOpacity` | 0.0~1.0 | 0.4 | 能层透明度 |
| `jetHelixStrength` | 0.0~2.0 | 1.0 | 喷流螺旋强度 |
| `geodesicSteps` | 48~256 | 128 | 测地线积分步数 |

### 4. 视觉效果

| a 值 | 爱因斯坦环 | 吸积盘 | 能层 | 喷流 |
|------|-----------|--------|------|------|
| 0.0 | 完美圆形 | 对称 | 无 | 直线 |
| 0.3 | 轻微椭圆 | 微弱不对称 | 薄壳 | 微螺旋 |
| 0.5 | 明显椭圆 | 30%亮度差 | 可见 | 螺旋 |
| 0.7 | 扭曲 | 2-3倍亮度差 | 明显 | 强螺旋 |
| 0.9 | 逗号状 | 极端不对称 | 厚壳 | 极强螺旋 |
| 0.99 | 单侧弧形 | 几乎触及视界 | 最厚 | 最强螺旋 |

## 技术细节

### 坐标系统

使用 **Boyer-Lindquist 坐标** (r, θ, φ)：
- r: 径向坐标
- θ: 极角（0 = 北极，π/2 = 赤道面，π = 南极）
- φ: 方位角（绕自转轴）

### 积分方法

使用 **4 阶 Runge-Kutta 方法**（RK4）进行测地线积分，保证数值稳定性。

### 守恒量

Kerr 时空中光子有 3 个守恒量：
1. **能量 E**（时间平移对称性）
2. **角动量 Lz**（绕自转轴）
3. **Carter 常数 Q**（Kerr 时空特有）

## 与现有代码的关系

- **独立于现有 `blackhole.glsl`**：修改 Kerr 不会影响施瓦茨柴尔德版本
- **共享 uniform 接口**：使用相同的 `frag_desktop_header.glsl` 头文件
- **可切换**：通过 `uKerrMode` uniform 在两种模式间切换

## 注意事项

- 自旋参数 a 限制在 0.99 以下，避免数值爆炸
- 极端自旋 (a>0.9) 时建议增加积分步数到 256
- 能层只在 a>0 时可见
- Kerr 模式性能约为施瓦茨柴尔德的 2-3 倍（128 步 vs 48 步）

## 参考

- Misner, Thorne, Wheeler "Gravitation" §33
- Teukolsky (2015) "The Kerr Metric"
- EHT Collaboration (2019-2023) M87* observations
- Eric Bruneton "Real-time High-Quality Rendering of Non-Rotating Black Holes"