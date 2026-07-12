# BlackHole — 实时引力透镜桌面屏保

> 基于 OpenGL 4.6 的施瓦茨柴尔德黑洞光线追踪可视化系统，叠加在实时桌面画面之上，支持交互式恒星演化、双黑洞系统、相对论性喷流等天体物理效果。

![BlackHole](https://img.shields.io/badge/OpenGL-4.6%20Core-blue) ![C++](https://img.shields.io/badge/C%2B%2B-17-orange) ![Platform](https://img.shields.io/badge/Platform-Windows-success) ![License](https://img.shields.io/badge/License-MIT-green)

---

## 目录

- [技术栈概览](#技术栈概览)
- [项目亮点](#项目亮点)
- [核心竞争力](#核心竞争力)
- [核心物理公式与算法](#核心物理公式与算法)
- [整体项目架构](#整体项目架构)
- [系统架构图](#系统架构图)
- [整体流程图](#整体流程图)
- [数据流架构](#数据流架构)
- [前端技术细节](#前端技术细节)
- [后端技术细节](#后端技术细节)
- [模块说明与功能模块设计](#模块说明与功能模块设计)
- [开发过程中遇到的问题及解决方法](#开发过程中遇到的问题及解决方法)
- [配置说明](#配置说明)
- [第三方服务集成](#第三方服务集成)
- [项目说明书](#项目说明书)
- [快速上手指南](#快速上手指南)
- [部署流程](#部署流程)
- [优化策略](#优化策略)
- [未来计划](#未来计划)
- [项目统计](#项目统计)
- [常见问题解答](#常见问题解答)
- [贡献指南](#贡献指南)
- [许可信息](#许可信息)

---

## 技术栈概览

| 类别 | 技术 | 版本/说明 |
|------|------|-----------|
| **图形 API** | OpenGL Core Profile | 4.6 (动态加载所有 GL 函数) |
| **编程语言** | C++ | C++17 标准 |
| **GUI 框架** | Dear ImGui | 面板内嵌实时调参 |
| **图像加载** | stb_image | 纹理加载 |
| **桌面捕获** | Windows Graphics Capture (WGC) | WinRT C ABI，兼容 MinGW |
| **窗口系统** | Win32 API + WGL | 分层透明窗口，点击穿透 |
| **构建系统** | CMake + MinGW Makefiles | GCC 12+ (MSYS2 UCRT64) |
| **着色器语言** | GLSL | #version 460 core |
| **系统库** | D3D11 / DXGI / DWMAPI | WGC 捕获 + DWM 合成 |

---

## 项目亮点

### 1. 真实的施瓦茨柴尔德黑洞渲染
不是贴图或简单遮罩，而是**逐像素数值积分测地线**。每个像素从相机发射一条光子，沿 Binet 形式的施瓦茨柴尔德度规积分 `a = -(3/2)h²x/r⁵`，真实地产生：
- 事件视界（光线被捕获的区域，你的桌面文字真的被吞噬了）
- 爱因斯坦环（光子球处的透镜放大）
- 吸积盘的多重成像（光线绕黑洞弯曲后，背面的盘也被看到）

### 2. 叠加在实时桌面之上
通过 WGC 捕获桌面画面作为纹理，黑洞效果实时扭曲你的桌面内容。你可以看到窗口图标被拉伸、文字在爱因斯坦环中镜像翻转。

### 3. 完整的恒星演化系统
左键创建恒星，右键触发三阶段演化：
- **普通恒星 → 白矮星**：膨胀 → 坍缩，形成碳氧晶体网格
- **白矮星 → 中子星**：坍缩 → 超新星爆炸 → 形成中子星（双极喷流 + 脉冲星灯塔效应）
- **中子星 → 黑洞**：中键坍缩超越奥本海默极限，形成小型黑洞

### 4. 双黑洞交互系统
小黑洞（中子星坍缩产物）与主黑洞之间实现四种交互效果：
- 引力波纹（空间扭曲圆形波纹）
- 轨道运动（弹簧力 + 引力 + 阻尼物理）
- 吸积桥（两黑洞间的湍流物质流）
- 蓝白色等离子能量流

### 5. 多层相对论性喷流
大黑洞两端的喷流采用 4 层结构：外鞘（射电瓣）+ 核心束（X 射线）+ 螺旋磁场线 + 内部结结构，模拟 M87 黑洞喷流的观测特征。

### 6. 7 种光谱类型恒星
基于 Morgan-Keenan 光谱分类，从深蓝色 O 型蓝色巨星到红色 M 型红矮星，颜色由黑体辐射温度决定。

---

## 核心竞争力

| 对比项 | 本项目 | 典型黑洞屏保 |
|--------|--------|-------------|
| 渲染方法 | 数值积分测地线（物理正确） | 贴图/简单遮罩 |
| 透镜效果 | 真实弯曲桌面内容 | 仅视觉效果 |
| 吸积盘 | 3D 光线-盘面交叉 + 多普勒光束 | 2D 贴图 |
| 交互性 | 鼠标创建恒星 + 演化 + 双黑洞 | 无交互 |
| 性能 | 60fps @ 2560x1440 | 依赖实现 |
| 桌面叠加 | 实时 WGC 捕获 | 截图或无 |
| 配置面板 | ImGui 60+ 实时参数 | 有限或无 |

---

## 核心物理公式与算法

### 1. 主黑洞 — 施瓦茨柴尔德测地线积分

**Binet 形式光子加速度**（`blackhole.glsl`）：

```
a = -(3/2) · h² · x / r⁵
```

其中 `h² = |pr|²` 是角动量平方（pr 为缩放后的像素偏移），`r = |x|` 是到原点距离。使用**leapfrog（半隐式欧拉）积分**：

```glsl
v += a * (0.5 * dt);
x += v * dt;
// 重新计算 a
v += a * (0.5 * dt);
```

**临界冲击参数**：
```
b_crit = (3√3 / 2) · r_s ≈ 2.598
```
当光线冲击参数 `b < b_crit` 时，光线被黑洞捕获（螺旋落入事件视界）。

**远场解析偏折**（大冲击参数时的近似公式，避免数值积分）：
```
u = Z₀ / √(Z₀² + b²)
deflection = (2 / W²) / |p| · (1.29·u + 0.07) · max(LENS_DEPTH - 2.14·u + 0.75, 0)
```

### 2. 吸积盘 — 开普勒盘 + 黑体辐射

**开普勒轨道速度**：
```
v_kepler ∝ (r_in / r)^1.5
```

**多普勒光束因子**：
```
g_loc = √(1 - 1.5·r_s/r)
β = 1/√(2(r-1))
g = g_loc / (1 + β·k̂·v̂)
```

**Shakura-Sunyaev 温度剖面**：
```
T(r) = T_in · (r_in/r)^0.75 · (1 - √(r_in/r))^0.25
```

**黑体辐射色温**：根据温度 T 计算颜色 (R,G,B)，使用普朗克黑体辐射近似。

### 3. 中子星 — 脉冲星灯塔效应

**磁轴方向**：
```
axis = (cos(tilt + rotation), sin(tilt + rotation))
```

**双极喷流锥体**：
```
along = dot(pixel_offset, axis)
perp = |pixel_offset - along · axis|
mask = exp(-perp² / (2·w²)) · smoothstep(0, jetLen, along)
```

**灯塔扫掠**：
```
sweep = pow(max(0, cos(rotation · pulseRate)), 8)
```

### 4. 超新星爆炸

**膨胀冲击壳**：
```
shockR = progress · maxRadius
shell = exp(-((r - shockR) / width)²)
```

**中心白闪**：
```
flash = (1 - progress) · exp(-progress · 3)
```

**16 条放射辐条**：
```
spokes = pow(0.5 + 0.5·cos(angle·16 + progress·10), 4)
```

### 5. 双黑洞轨道物理

**弹簧力**（鼠标跟随）：
```
F_spring = k · (mousePos - bhPos)
```

**主黑洞引力**（反平方律）：
```
F_grav = G / (r² + ε)
```

**指数阻尼**：
```
v *= exp(-damping · dt)
```

**半隐式欧拉积分**：
```
v += F · dt
x += v · dt
```

### 6. 能量流 — 贝塞尔曲线 + 螺旋盘绕

**三次贝塞尔曲线**：
```
B(t) = (1-t)³·P₀ + 3(1-t)²t·P₁ + 3(1-t)t²·P₂ + t³·P₃
```

**螺旋盘绕**（到达黑洞后）：
```
spiral(θ) = BH_center + (r₀ · e^(-kθ)) · (cos(θ), sin(θ))
```

### 7. 帧缓冲引力透镜

小黑洞使用 `glCopyTexImage2D` 捕获渲染后的帧缓冲，在 shader 中用施瓦茨柴尔德偏折公式采样偏移位置：
```
deflection ∝ R / r
sampleUV = originalUV - direction · deflection
```

---

## 整体项目架构

```
BlackHole/
├── src/                          # C++ 源代码
│   ├── main.cpp                  # 主程序入口、渲染循环、交互系统 (1968 行)
│   ├── win32_gl.h/.cpp           # Win32 OpenGL 窗口 + WGL 上下文
│   ├── capture_wgc.h/.cpp        # WGC 桌面捕获 (WinRT C ABI)
│   ├── gl_texture.h/.cpp         # OpenGL 纹理上传
│   ├── monitors.h/.cpp           # 多显示器枚举
│   ├── gui_config.h/.cpp         # 配置系统 + ImGui 面板 (823 行)
│   ├── star_texture.h/.cpp       # 恒星纹理生成
│   ├── imgui/                    # Dear ImGui 库
│   └── ...
├── shaders/                      # GLSL 着色器
│   ├── vert.glsl                 # 全屏四边形顶点着色器
│   ├── frag_desktop_header.glsl  # 桌面模式 fragment 头 (uniform 声明)
│   ├── blackhole.glsl            # 主黑洞测地线追踪核心 (474 行)
│   ├── star_vert.glsl            # 恒星顶点着色器
│   ├── star_frag.glsl            # 恒星片元着色器 (874 行，含白矮星/中子星/小黑洞)
│   ├── energy_stream_vert.glsl   # 能量流顶点着色器
│   ├── energy_stream_frag.glsl   # 能量流片元着色器 (贝塞尔曲线 + 螺旋)
│   ├── particle_vert.glsl        # 粒子顶点着色器
│   ├── particle_frag.glsl        # 粒子片元着色器
│   ├── star_wind_vert/frag.glsl  # 太阳风着色器
│   └── frag_header/preview_header.glsl  # 其他模式头文件
├── blackhole.glsl                # 主黑洞 shader（根目录，编译时复制到 build/）
├── texture/                      # 纹理资源
├── stb-master/                   # stb_image 库
├── CMakeLists.txt                # 构建配置
├── resource.rc                   # Windows 资源文件
└── .trae/documents/              # 项目计划文档
```

---

## 系统架构图

```
┌─────────────────────────────────────────────────────────────┐
│                        BlackHole.exe                         │
│                                                               │
│  ┌─────────────┐   ┌──────────────┐   ┌───────────────────┐ │
│  │  Win32 窗口  │   │  WGC 桌面捕获 │   │   ImGui 配置面板  │ │
│  │  (透明分层)  │   │  (D3D11 设备) │   │   (F1 开关)       │ │
│  └──────┬──────┘   └───────┬──────┘   └────────┬──────────┘ │
│         │                  │                    │            │
│         ▼                  ▼                    ▼            │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │                    OpenGL 渲染管线                        │ │
│  │                                                          │ │
│  │  1. 桌面纹理 → GL_TEXTURE0                               │ │
│  │  2. blackhole.glsl (测地线追踪 + 吸积盘 + 喷流)          │ │
│  │  3. glCopyTexImage2D → 帧缓冲捕获 → GL_TEXTURE1          │ │
│  │  4. star_frag.glsl (恒星/白矮星/中子星/小黑洞)           │ │
│  │  5. 粒子系统 (吸积流)                                     │ │
│  │  6. energy_stream_frag.glsl (能量流)                    │ │
│  │  7. ImGui 面板                                           │ │
│  │  8. SwapBuffers → DWM 合成到桌面                          │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │                    交互系统                              │ │
│  │  左键 → 创建恒星    右键 → 演化    中键 → 坍缩成黑洞     │ │
│  └──────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
              ┌───────────────────┐
              │   Windows DWM     │
              │   (桌面合成器)     │
              └───────────────────┘
```

---

## 整体流程图

```
程序启动
  │
  ├─ Win32GL_Init()         创建透明分层窗口
  ├─ LoadGLFunctions()      动态加载 OpenGL 4.6 函数
  ├─ WGC_Init()             初始化桌面捕获
  ├─ CompileShaders()       编译所有 shader (黑洞/恒星/能量流/粒子)
  ├─ InitStars()            初始化恒星数组
  ├─ Config_Load()          加载配置文件
  │
  └─ 主循环 ──────────────────────────────────────────────┐
       │                                                   │
       ├─ WGC_GetFrame()      捕获桌面画面                  │
       ├─ GLTex_Upload()      上传到 GL_TEXTURE0            │
       ├─ Config_SmoothUpdate() 平滑参数过渡                │
       │                                                   │
       ├─ 更新恒星状态机                                    │
       │   ├─ 左键创建 / 长按增长 / 松开消散               │
       │   ├─ 右键演化 (膨胀→坍缩→超新星→中子星)          │
       │   ├─ 中键坍缩 (中子星→小黑洞)                    │
       │   └─ 小黑洞轨道物理 (弹簧+引力+阻尼)              │
       │                                                   │
       ├─ 渲染背景 (blackhole.glsl)                        │
       │   ├─ 测地线追踪 (48 步 leapfrog)                  │
       │   ├─ 吸积盘交叉检测                                │
       │   ├─ 远场解析偏折                                  │
       │   ├─ 喷流渲染 (4 层)                              │
       │   └─ 屏幕抖动 + 暗角 + 张力线                     │
       │                                                   │
       ├─ glCopyTexImage2D()   捕获帧缓冲→TEXTURE1         │
       │                                                   │
       ├─ 渲染恒星 (star_frag.glsl)                        │
       │   ├─ body pass (alpha 混合)                       │
       │   └─ emissive pass (加法混合 GL_ONE,GL_ONE)       │
       │                                                   │
       ├─ 渲染粒子系统                                      │
       ├─ 渲染能量流                                        │
       ├─ GUI_Render() (如果面板打开)                      │
       ├─ SwapBuffers()                                    │
       │                                                   │
       └─ 检测 ESC / F1 / 鼠标输入 ──────────────────────→│
                                                         │
       ESC → 退出                                        │
       F1  → 切换 GUI 面板                                │
```

---

## 数据流架构

```
┌──────────┐     BGRA 纹理      ┌──────────────┐
│ WGC 捕获  │ ──────────────────→│ GL_TEXTURE0  │
│ (D3D11)  │                    └──────┬───────┘
└──────────┘                           │
                                       ▼
┌──────────────────────────────────────────────────┐
│           blackhole.glsl (fragment shader)       │
│  iChannel0 = texture(uTexture0, mirrorUV(uv))    │
│  → 测地线追踪 → 扭曲后的背景画面                   │
└──────────────────────┬───────────────────────────┘
                       │ 渲染结果写入帧缓冲
                       ▼
              ┌─────────────────┐
              │ glCopyTexImage2D │
              │ → GL_TEXTURE1   │
              └────────┬────────┘
                       │
                       ▼
┌──────────────────────────────────────────────────┐
│           star_frag.glsl (fragment shader)       │
│  uFBTexture = texture(uTexture1, lensedUV)      │
│  → 小黑洞引力透镜采样扭曲后的背景                  │
└──────────────────────┬───────────────────────────┘
                       │
                       ▼
┌──────────────────────────────────────────────────┐
│        energy_stream_frag.glsl                   │
│  → 贝塞尔曲线能量流渲染                           │
└──────────────────────┬───────────────────────────┘
                       │
                       ▼
                 SwapBuffers
                       │
                       ▼
              ┌──────────────┐
              │  DWM 合成器   │
              │  → 桌面显示   │
              └──────────────┘
```

**数据流关键点**：
- 桌面画面 → `GL_TEXTURE0` → 黑洞 shader 采样扭曲
- 黑洞渲染结果 → `glCopyTexImage2D` → `GL_TEXTURE1` → 恒星 shader 采样（小黑洞透镜）
- 配置参数通过 uniform 实时注入所有 shader
- 鼠标输入 → 恒星状态机 → uniform 位置/大小参数

---

## 前端技术细节

> 本项目为原生 Win32 + OpenGL 应用，"前端"指渲染层和用户交互层。

### 渲染管线顺序

```
1. glClear(COLOR_BUFFER_BIT)
2. 桌面纹理上传 → GL_TEXTURE0
3. blackhole.glsl (全屏四边形)
   ├─ 测地线积分 (近场: 48 步 leapfrog)
   ├─ 解析偏折 (远场)
   ├─ 吸积盘交叉检测 + 黑体辐射
   ├─ 相对论性喷流 (4 层)
   └─ 屏幕抖动/暗角/张力线
4. glCopyTexImage2D → GL_TEXTURE1 (帧缓冲捕获)
5. star_frag.glsl (每颗星)
   ├─ body pass: alpha 混合 (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
   └─ emissive pass: 加法混合 (GL_ONE, GL_ONE) — 仅 NS/超新星/BH 阶段
6. particle_vert/frag.glsl (粒子点精灵)
7. energy_stream_frag.glsl (能量流)
8. ImGui 渲染 (如果面板打开)
9. SwapBuffers
```

### 着色器动态编译

所有 shader 在运行时编译，支持**源码修补**：
- 读取 header + body → 字符串拼接
- 7 项文本替换：`const` 值替换为 `uniform < 0 ? const_val : uniform`
- 负值 uniform 表示"使用 shader 默认值"

### 双 pass 渲染系统

```cpp
// body pass: 正常渲染
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
glUniform1i(suPassMode, 0);
glDrawArrays(...);

// emissive pass: 发射体加法混合 (仅特定阶段)
if (needEmissive) {
    glBlendFunc(GL_ONE, GL_ONE);
    glUniform1i(suPassMode, 1);
    glDrawArrays(...);
}
```

### 鼠标交互

| 操作 | 效果 |
|------|------|
| 左键 | 创建恒星（随机光谱类型），长按变大 |
| 右键 | 触发恒星演化（白矮星→中子星） |
| 中键 | 中子星坍缩成小黑洞 |
| F1 | 切换 ImGui 配置面板 |
| ESC | 退出程序 |

---

## 后端技术细节

> "后端"指 C++ 逻辑层：窗口管理、桌面捕获、状态机、物理模拟。

### Win32 透明分层窗口

```cpp
// 窗口样式：始终置顶 + 点击穿透 + 不激活
WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT
WS_POPUP

// WndProc 关键处理：
WM_NCHITTEST → HTTRANSPARENT     // 鼠标穿透
WM_MOUSEACTIVATE → MA_NOACTIVATEANDEAT  // 不抢焦点

// GUI 面板激活时动态移除 WS_EX_TRANSPARENT，恢复点击
```

### WGC 桌面捕获管线

```
1. D3D11CreateDevice(HARDWARE, BGRA_SUPPORT)
2. RoInitialize(RO_INIT_MULTITHREADED)
3. CreateDirect3D11DeviceFromDXGIDevice → IDirect3DDevice
4. IGraphicsCaptureItemInterop::CreateForMonitor(hMon) → CaptureItem
5. Direct3D11CaptureFramePool.Create(BGRA8, 3 buffers)
6. IGraphicsCaptureSession.StartCapture()
7. Win11 22H2+: put_IsBorderRequired(false) — 抑制黄框
8. CPU Readback: CopyResource(Staging) → Map → GLTex_Upload(BGRA)
```

### 恒星演化状态机

```
enum StarStage { STAGE_NORMAL, STAGE_WHITE_DWARF, STAGE_NEUTRON_STAR, STAGE_BLACK_HOLE };
enum StarEvo   { EVO_IDLE, EVO_EXPANDING, EVO_COLLAPSING, EVO_PRE_SUPERNOVA, EVO_SUPERNOVA, EVO_SETTLE };

右键按下 → 检查当前 stage:
  NORMAL    → EVO_EXPANDING → EVO_COLLAPSING → WHITE_DWARF (evo=IDLE)
  WHITE_DWARF → EVO_COLLAPSING → EVO_PRE_SUPERNOVA → EVO_SUPERNOVA → NEUTRON_STAR (evo=SETTLE→IDLE)
  NEUTRON_STAR (中键) → EVO_COLLAPSING → BLACK_HOLE (evo=SETTLE→IDLE)
```

### 小黑洞轨道物理

```cpp
if (s.stage == STAGE_BLACK_HOLE && s.evo == EVO_IDLE && config.dualBHEnabled) {
    // 弹簧力：朝鼠标位置
    float dx = mouseX - s.x, dy = mouseY - s.y;
    s.vx += dx * config.miniBHMouseFollow * dt;
    s.vy += dy * config.miniBHMouseFollow * dt;
    
    // 主黑洞引力（反平方律，近距离生效）
    float bhDist2 = (bhCx - s.x)² + (bhCy - s.y)²;
    if (bhDist > 0.01f && bhDist < 0.5f) {
        float grav = config.miniBHGravity / (bhDist2 + 0.001f);
        s.vx += (bhCx - s.x) / bhDist * grav * dt;
        s.vy += (bhCy - s.y) / bhDist * grav * dt;
    }
    
    // 指数阻尼
    float damp = expf(-config.miniBHDamping * dt);
    s.vx *= damp; s.vy *= damp;
    
    // 半隐式欧拉积分
    s.x += s.vx * dt; s.y += s.vy * dt;
}
```

### 帧缓冲引力透镜

```cpp
// 背景渲染完成后、恒星渲染前
if (hasMiniBH) {
    if (!allocated) {
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, w, h, 0);
    } else {
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
    }
}
// 绑定到 GL_TEXTURE1，传给 star shader 的 uFBTexture
```

---

## 模块说明与功能模块设计

### 模块 1：主黑洞渲染 (`blackhole.glsl`)

**功能**：施瓦茨柴尔德黑洞的测地线追踪渲染。

**核心算法**：
- 48 步 leapfrog 积分，自适应步长 `dt = clamp(0.16·r, 0.03, 1.5)`
- 盘面交叉检测：`dot(x, n) · dot(xPrev, n) < 0` → 插值求交点
- 远场解析偏折（`b > bmax` 时跳过积分，直接用近似公式）
- 14 个可调 uniform + 6 个内置预设

**内置预设**：人马座 A*、M87*、天鹅座 X-1、卡尔冈图雅（星际穿越）、微型黑洞、原初黑洞

### 模块 2：恒星系统 (`star_frag.glsl` + `main.cpp`)

**功能**：交互式恒星创建、渲染、演化。

**子模块**：
| 子模块 | 功能 | 着色器函数 |
|--------|------|-----------|
| 光球层 | 恒星表面颗粒纹理 | `photosphereColor()` |
| 日冕 | 外围珍珠白发光 | 径向渐变 + 噪声 |
| 色球层 | 边缘红色薄环 | Fresnel 边缘光 |
| 太阳风 | 向外辐射粒子 | CPU 粒子系统 |
| 引力扭曲 | 背景微小偏移 | star lensing uniform |

### 模块 3：恒星演化 (`star_frag.glsl`)

| 阶段 | 渲染函数 | 视觉特征 |
|------|----------|----------|
| 白矮星 | `whiteDwarfColor()` | Voronoi 碳氧晶体网格，白色辉光 |
| 中子星 | `neutronStarColor()` | 极高频 ridged voronoi 裂纹，蓝紫色 |
| 中子星喷流 | `nsJets()` | 沿磁轴双锥体，X 射线蓝紫色 |
| 脉冲星灯塔 | `nsPulsarBeam()` | 旋转扫掠光束 |
| 超新星 | `supernovaShell()` | 膨胀冲击壳 + 16 条放射辐条 + 中心白闪 |
| 小黑洞 | `miniBHColor()` | 测地线追踪（与主黑洞相同管线） |

### 模块 4：能量流 (`energy_stream_frag.glsl`)

**功能**：恒星 → 黑洞的物质能量流。

**核心**：
- 三次贝塞尔曲线路径
- 到达黑洞后螺旋盘绕（1.5~2.5 圈）
- 4 层 FBM 噪声燃烧纹理
- 加法混合（`GL_SRC_ALPHA, GL_ONE`），亮度 ×5.0
- 颜色随恒星阶段变化：NORMAL=恒星色，WD=白蓝，NS=紫蓝，BH=蓝白等离子

### 模块 5：粒子系统 (`particle_vert/frag.glsl`)

**功能**：吸积盘物质流的 3D 粒子。

**物理**：
- CPU 更新位置（引力 + 切向速度）
- GPU 点精灵渲染（深度衰减大小）
- 最多 3000 粒子 + 64 段拖尾

### 模块 6：双黑洞系统

| 效果 | 实现方式 |
|------|----------|
| 引力波纹 | 3 个从中点扩展的圆形波纹，采样帧缓冲施加径向位移 |
| 轨道运动 | 弹簧力 + 引力 + 阻尼 + 半隐式欧拉 |
| 吸积桥 | 两黑洞间纺锤形湍流物质流 |
| 能量流连接 | 蓝白等离子色，亮度 ×1.5 |

### 模块 7：配置系统 (`gui_config.h/.cpp`)

**功能**：60+ 可调参数，ImGui 实时面板，文件持久化。

**配置分组**：
- 基本参数（洞半径、曝光、速度）
- 高级吸积盘（内外径、不透明度、多普勒、光束、对比度）
- 相对论喷流（长度、宽度、速度、亮度）
- 粒子系统（数量、大小、螺旋、引力、颜色）
- 恒星系统（大小、增长、日冕、太阳风）
- 恒星演化（坍缩速度、超新星、白矮星、中子星）
- 双黑洞系统（透镜强度、引力波、吸积桥）

---

## 开发过程中遇到的问题及解决方法

### 问题 1：能量流被吸积盘蓝色覆盖

**现象**：能量流只在出现几秒后消失，或被黑洞吸积盘的蓝色完全覆盖。

**原因**：渲染顺序和混合模式不正确。

**解决**：
- 能量流渲染在黑洞 shader 之后（后绘制 = 覆盖）
- 使用加法混合 `glBlendFunc(GL_SRC_ALPHA, GL_ONE)`，亮度 ×5.0
- 能量流颜色使用饱和橙红色，与蓝色吸积盘形成对比

**效果**：能量流成为场景中最亮的元素，始终可见。

### 问题 2：中键坍缩不工作

**现象**：中子星右键演化到一半后，中键无法触发坍缩成黑洞。

**原因**：超新星爆炸进行到一半时（`supernovaProgress > 0.5`），stage 从白矮星切换到中子星，但超新星进度处理器在白矮星分支里。切换后代码进入中子星分支，那里没有 `EVO_SUPERNOVA` 处理器，导致超新星永远无法完成、`evo` 卡在 `EVO_SUPERNOVA`。

**解决**：在中子星分支新增 `EVO_SUPERNOVA` 处理器继续推进超新星到完成；中子星的 `EVO_SETTLE` 改为简单等待 0.6s 后稳定。

**效果**：右键演化到中子星后，中键可正常触发坍缩成黑洞。

### 问题 3：小黑洞引力透镜不够强

**现象**：小黑洞的引力透镜效果与主黑洞差距很大，没有"挤压空间"的感觉。

**原因**：缺少 `B_CRIT` 缩放。主黑洞使用 `W = B_CRIT / rh` 将像素偏移缩放到物理正确的局部坐标，而小黑洞直接用 `d / R`，引力场弱了 2.6 倍。

**解决**：完全对齐主黑洞的渲染管线：
- `W = B_CRIT / R`（临界冲击参数缩放）
- `Z0 = max(14.0, rout + 5.0)`（相机距离）
- 64 步 leapfrog 积分 + 远场解析偏折
- `LENS_DEPTH = 13.0`

**效果**：小黑洞的引力透镜效果与主黑洞一致，能真实扭曲背景内容。

### 问题 4：小黑洞显示为扁椭圆形

**现象**：小黑洞本体不是圆形，而是被压扁的椭圆。

**原因**：`main()` 中 `d.x *= aspect` 拉伸了 x 方向，但 `miniBHColor` 内部又除以 aspect 还原，导致在 UV 空间的圆形在屏幕上变成扁椭圆。

**解决**：全部改用 aspect-corrected 的 `d` 坐标系，事件视界用 `nr < 1.0` 判定（`nr` 基于 `d`），确保屏幕上是正圆。

### 问题 5：恒星颜色只有红橙色

**现象**：即使创建了 O 型蓝色巨星，显示的仍是红橙色。

**原因**：`star_frag.glsl` 中恒星颜色基底是硬编码的红色/橙色：
```glsl
vec3 darkBase = vec3(0.5, 0.05, 0.0);   // deep red
vec3 midBase = vec3(1.0, 0.35, 0.05);  // orange
```
虽然后面有 `col *= uStarColor`，但红底 × 蓝色 = 暗紫色。

**解决**：基底色直接从 `uStarColor` 派生：
```glsl
vec3 darkBase = uStarColor * 0.35;
vec3 midBase = uStarColor * 0.85;
vec3 brightBase = mix(uStarColor, vec3(1.0), 0.6);
```

**效果**：蓝色巨星显示为蓝色，红矮星显示为红色，颜色多样性正常。

### 问题 6：Shader 编译错误导致恒星创建失效

**现象**：左键点击无法创建恒星。

**原因**：`star_frag.glsl` 中 `ridgedVoronoi(vec2)` 被传入了 `vec3` 参数，导致 shader 编译失败，`starProgram = 0`。

**解决**：将 `np * freq + vec3(...)` 改为 `np.xy * freq + vec2(...)`。

### 问题 7：吸积盘没有 3D 效果

**现象**：小黑洞吸积盘是平面圆形，没有 3D 透视感。

**原因**：使用 2D 逆投影而非 3D 光线追踪。

**解决**：改用与主黑洞相同的 3D 光线-盘面交叉方法：盘面法向量 `n = (0, sin(incl), cos(incl))`，从相机发射光线与倾斜盘面求交，计算交点处的 3D 半径、角度、温度。

---

## 配置说明

### 配置文件

| 文件 | 说明 |
|------|------|
| `blackhole_presets.txt` | 预设参数文件 |
| `blackhole_advanced.ini` | 高级设置 |

### 主要配置参数

#### 黑洞基本参数
| 参数 | 默认值 | 范围 | 说明 |
|------|--------|------|------|
| `holeRadius` | 0.02 | 0.005-0.1 | 黑洞视界半径 |
| `exposure` | 1.4 | 0.1-5.0 | 曝光强度 |
| `speed` | 1.0 | -5.0~5.0 | 时间流速（负=倒流） |

#### 吸积盘参数
| 参数 | 默认值 | 说明 |
|------|--------|------|
| `diskTemp` | 5500 | 盘温度 (K) |
| `diskIncl` | 1.5 | 倾角 (弧度) |
| `diskInner` | 1.8 | 内半径 (r_s) |
| `diskOuter` | 8.0 | 外半径 (r_s) |
| `diskOpac` | 0.9 | 不透明度 |
| `dopplerMix` | 0.6 | 多普勒效应混合 |
| `diskBeam` | 2.5 | 相对论光束因子 |
| `diskGain` | 2.2 | 亮度增益 |

#### 恒星演化参数
| 参数 | 默认值 | 说明 |
|------|--------|------|
| `evoExpandSpeed` | 0.25 | 膨胀速度 |
| `evoCollapseSpeed` | 0.35 | 坍缩速度 |
| `supernovaBrightness` | 2.5 | 超新星亮度 |
| `supernovaDuration` | 1.2 | 超新星持续 (秒) |
| `nsRotationSpeed` | 6.0 | 中子星自转速度 |
| `nsJetLength` | 0.9 | 喷流长度 |

#### 双黑洞系统参数
| 参数 | 默认值 | 说明 |
|------|--------|------|
| `dualBHEnabled` | true | 双黑洞开关 |
| `miniBHMouseFollow` | 8.0 | 鼠标跟随弹簧常数 |
| `miniBHGravity` | 0.002 | 主黑洞引力常数 |
| `miniBHDamping` | 2.0 | 速度阻尼 |
| `miniBHLensStrength` | 3.0 | 透镜强度 |
| `gwRippleStrength` | 0.5 | 引力波纹强度 |
| `accretionBridgeBright` | 1.0 | 吸积桥亮度 |

---

## 第三方服务集成

| 服务/库 | 用途 | 集成方式 |
|---------|------|---------|
| **Windows Graphics Capture** | 桌面画面捕获 | WinRT C ABI 手动声明 IID（兼容 MinGW） |
| **Direct3D 11** | WGC 捕获的 GPU 设备 | D3D11CreateDevice + IDXGIDevice |
| **DWM (Desktop Window Manager)** | 分层窗口合成 | DwmExtendFrameIntoClientArea |
| **Dear ImGui** | 配置面板 GUI | 静态链接，src/imgui/ |
| **stb_image** | 纹理加载 | 头文件包含，stb-master/ |
| **Win32 API** | 窗口、输入、显示器 | 原生 API 调用 |

---

## 项目说明书

### 项目定位

BlackHole 是一个 **Windows 桌面增强应用**，将物理正确的黑洞渲染叠加在实时桌面画面之上。它不是传统的屏保（虽然支持空闲触发），而是一个**实时天体物理可视化工具**，让用户在桌面上体验引力透镜、恒星演化、双黑洞交互等天文现象。

### 目标用户

- 天文爱好者和学生
- 桌面美化爱好者
- OpenGL/Shader 开发者（学习参考）
- 天体物理可视化研究者

### 核心功能

1. **实时引力透镜**：桌面内容被真实弯曲
2. **恒星创建与演化**：左键创建，右键演化三阶段
3. **双黑洞系统**：小黑洞跟随鼠标，与主黑洞交互
4. **60+ 实时参数**：ImGui 面板即时调节
5. **6 个内置预设**：人马座 A*、M87*、天鹅座 X-1 等
6. **多显示器支持**：可选择捕获哪个显示器
7. **空闲自动启动**：可配置空闲超时

---

## 快速上手指南

### 环境要求

- Windows 10 1903+ / Windows 11
- OpenGL 4.6 兼容显卡
- MSYS2 UCRT64 环境（开发）

### 构建步骤

```bash
# 1. 安装 MSYS2 和 UCRT64 工具链
# https://www.msys2.org/

# 2. 安装依赖
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake

# 3. 克隆项目
git clone https://github.com/yourname/BlackHole.git
cd BlackHole

# 4. 构建配置
cmake -B build -G "MinGW Makefiles"

# 5. 编译
cmake --build build

# 6. 运行
./build/blackhole.exe
```

### 基本操作

| 操作 | 效果 |
|------|------|
| 启动 | 黑洞自动出现在桌面右下角 |
| 左键点击 | 创建恒星（随机光谱类型） |
| 长按左键 | 恒星持续变大 |
| 松开左键 | 恒星消散 |
| 右键 | 触发恒星演化（膨胀→白矮星→超新星→中子星） |
| 中键 | 中子星坍缩成小黑洞 |
| F1 | 打开/关闭配置面板 |
| ESC | 退出 |

### 演化流程

```
左键创建恒星
    ↓
右键 #1: 膨胀 → 坍缩 → 白矮星
    ↓
右键 #2: 坍缩 → 超新星爆炸 → 中子星 (双极喷流)
    ↓
中键: 坍缩 → 小黑洞 (跟随鼠标, 与主黑洞交互)
```

---

## 部署流程

### 开发环境部署

```bash
# 环境变量
set PATH=C:\msys64\ucrt64\bin;%PATH%

# 配置 + 编译
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 构建产物
# build/blackhole.exe        — 主程序
# build/shaders/*.glsl        — 着色器（自动复制）
# build/texture/*             — 纹理（自动复制）
# build/blackhole.glsl        — 主黑洞着色器
```

### 分发部署

```bash
# 1. Release 构建
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. 打包所需文件
# blackhole.exe
# shaders/ 目录
# blackhole.glsl
# texture/ 目录
```

### 可选：D3D11 渲染路径

```bash
cmake -B build -G "MinGW Makefiles" -DBLACKHOLE_USE_D3D11=ON
```

---

## 优化策略

### 渲染性能

| 策略 | 效果 |
|------|------|
| 远场解析偏折公式 | 大冲击参数时跳过 48 步积分，直接用近似公式 |
| 自适应步长 `dt = clamp(0.16·r, 0.03, 1.5)` | 近黑洞精细，远处粗略 |
| `discard` 提前退出 | 像素远离天体时跳过计算 |
| 最多 3 颗恒星 | 限制渲染开销 |
| 帧缓冲捕获仅在有 mini BH 时 | 避免不必要的 glCopyTexSubImage2D |
| ImGui 仅在面板打开时渲染 | 减少不必要 draw call |

### 着色器优化

- **N_STEPS = 48**：平衡精度与性能
- **能量流 discard**：大部分像素提前退出
- **FBM 限制 4 层**：避免过多噪声迭代
- **点精灵粒子**：GPU 原生支持，开销低

### 内存优化

- `glCopyTexSubImage2D` 复用已分配纹理（首帧 `glCopyTexImage2D` 分配，后续更新）
- 粒子池复用（dead 粒子 slot 重用）
- 配置文件简单文本格式，无 JSON 依赖

---

## 未来计划

- [ ] **自旋黑洞 (Kerr 度规)**：当前为施瓦茨柴尔德（非自旋），未来支持 Kerr 度规的帧拖曳效应
- [ ] **Qt6 QML 配置界面**：独立进程提供可视化配置 + FBO 预览 + 系统托盘
- [ ] **D3D11 零拷贝管线**：WGC → ID3D11Texture2D → HLSL → SwapChain，消除 CPU readback
- [ ] **多屏穿梭**：黑洞在多个显示器之间移动
- [ ] **定时计划**：预设时间自动切换不同黑洞配置
- [ ] **音频反应**：黑洞参数响应系统音频
- [ ] **VR 支持**：OpenVR 头显沉浸式黑洞体验
- [ ] **黑洞合并模拟**：两个黑洞的引力波合并事件

---

## 项目统计

| 指标 | 数值 |
|------|------|
| **总代码行数** | ~6,019 行 |
| **C++ 代码** | ~4,185 行 |
| **GLSL Shader** | ~1,834 行 |
| **源文件数** | 27 个 (.cpp/.h/.glsl) |
| **着色器数量** | 12 个 GLSL 文件 |
| **可调参数** | 60+ 个 |
| **内置预设** | 6 个 (人马座 A*、M87* 等) |
| **恒星光谱类型** | 7 种 (O/B/A/F/G/K/M) |
| **恒星演化阶段** | 4 个 (恒星→白矮星→中子星→黑洞) |
| **最大恒星数** | 3 颗同时存在 |
| **最大粒子数** | 3,000 个 + 1,500 个太阳风 |
| **测地线积分步数** | 48 步 (主黑洞) / 64 步 (小黑洞) |
| **构建系统** | CMake 3.20+ |
| **最低 OpenGL** | 4.6 Core |

---

## 常见问题解答

### Q1: 启动后看不到黑洞？

**A**: 黑洞默认出现在桌面右下角，非常小。按 F1 打开配置面板，增大「洞半径」参数。

### Q2: 左键点击没反应？

**A**: 检查控制台输出是否有 shader 编译错误。如果 `starProgram = 0`，说明 star_frag.glsl 编译失败，恒星系统被禁用。

### Q3: 桌面画面不更新？

**A**: WGC 捕获需要 Windows 10 1903+。检查 `captureMode` 配置，`-1` 为自动选择。

### Q4: 性能很低？

**A**: 
- 降低分辨率
- 减少粒子数量
- 关闭双黑洞系统
- 减小 `LENS_DEPTH` 参数

### Q5: 中键坍缩不工作？

**A**: 确保先右键演化到中子星阶段（会看到超新星爆炸），然后等中子星稳定后再中键。如果 `evo` 卡在 `EVO_SUPERNOVA`，是已修复的 bug，确保使用最新版本。

### Q6: 小黑洞引力透镜不够强？

**A**: 在 GUI 中调节「透镜强度」(`miniBHLensStrength`)，默认 3.0，可增大到 5.0+。

### Q7: 如何保存自定义预设？

**A**: F1 打开面板 → 调节参数 → 点击「保存预设」→ 输入名称 → 保存。预设保存在 `blackhole_presets.txt`。

### Q8: 黑洞背景显示黑色而非桌面？

**A**: WGC 捕获失败。检查是否有其他屏幕录制软件占用，或尝试切换 `captureMode`。

---

## 贡献指南

### 开发环境搭建

1. Fork 本仓库
2. 安装 MSYS2 UCRT64 工具链
3. 按照快速上手指南构建项目
4. 确认 `blackhole.exe` 能正常运行

### 代码规范

- C++17 标准
- GLSL `#version 460 core`
- 4 空格缩进
- 函数名 camelCase
- 常量 UPPER_CASE
- Shader 函数名 camelCase

### 提交规范

```
<type>(<scope>): <subject>

<body>

<footer>
```

类型：`feat`（新功能）、`fix`（修复）、`refactor`（重构）、`docs`（文档）、`shader`（着色器）

### PR 流程

1. 创建功能分支：`git checkout -b feat/your-feature`
2. 提交更改
3. 推送并创建 PR
4. 确保构建通过且无 shader 编译错误

### 贡献领域

- **物理精度**：改进测地线积分、支持 Kerr 度规
- **视觉效果**：新天体类型、更好的纹理
- **性能优化**：D3D11 零拷贝、GPU compute
- **跨平台**：Linux/Mac 移植
- **文档**：改进 README、添加教程

---

## 许可信息

本项目基于 **MIT 许可证** 开源。

```
MIT License

Copyright (c) 2026 BlackHole Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### 致谢

- **Eric Bruneton** — [Real-time High-Quality Rendering of Non-Rotating Black Holes](https://ebruneton.github.io/black_hole_shader/) — 原始黑洞渲染算法
- **Ghostty** — blackhole.glsl 最初基于 Ghostty 的黑洞 shader
- **Dear ImGui** — 即时模式 GUI 库
- **stb** — 单头文件图像加载库
