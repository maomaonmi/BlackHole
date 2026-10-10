# Kerr 度规数学推导

## 1. Kerr 度规（Boyer-Lindquist 形式）

时空线元：

```
ds² = -(1 - 2Mr/Σ) dt² - (4Mar sin²θ/Σ) dt dφ
    + (Σ/Δ) dr² + Σ dθ²
    + (r² + a² + 2Ma²r sin²θ/Σ) sin²θ dφ²
```

其中：
- `Σ = r² + a² cos²θ`
- `Δ = r² - 2Mr + a²`
- M = 黑洞质量（归一化到 1）
- a = J/M = 角动量参数（0 ≤ a ≤ 1）

## 2. 关键曲面

### 2.1 事件视界

Δ = 0 的解：

```
r₊ = M + √(M² - a²)   (外视界)
r₋ = M - √(M² - a²)   (内视界，柯西视界)
```

当 a = 0 时，r₊ = 2M，退化为施瓦茨柴尔德半径。
当 a → 1 时，r₊ → M，极端 Kerr 黑洞。

### 2.2 静态极限面（能层边界）

g_tt = 0 的解：

```
r_ergo(θ) = M + √(M² - a² cos²θ)
```

在赤道面 (θ = π/2)：r_ergo = 2M（与施瓦茨柴尔德半径相同）
在极点 (θ = 0)：r_ergo = M + √(M² - a²) = r₊

能层区域：r₊ < r < r_ergo(θ)

## 3. 光子测地线方程

### 3.1 守恒量

对光子（零测地线），有 3 个守恒量：

1. **能量**: E = -p_t（时间 Killing 矢量）
2. **角动量**: Lz = p_φ（轴对称 Killing 矢量）
3. **Carter 常数**: Q = p_θ² + cos²θ(a²(m² - E²) + Lz²/sin²θ)

对于光子 (m = 0)，定义：
- `b = Lz/E`（冲击参数，方位角方向）
- `q = Q/E²`（Carter 常数/能量²）

### 3.2 一阶运动方程

```
Σ² (dr/dλ)² = R(r)
  = (r² + a² - a b)² - Δ[(b - a)² + q]

Σ² (dθ/dλ)² = Θ(θ)
  = q + cos²θ(a² - b²/sin²θ)

Σ (dφ/dλ) = b/sin²θ + a(2r - a b)/Δ

Σ (dt/dλ) = a(b - a sin²θ) + (r² + a²)(r² + a² - a b)/Δ
```

### 3.3 从笛卡尔坐标初始化

对于从无穷远来的光子，初始条件：
- 位置：r₀ → ∞，θ₀ = π/2（赤道面）
- 速度：指向黑洞方向

冲击参数 b 与笛卡尔坐标的关系：
- b_x = 冲击参数在 x 方向的分量
- b_y = 冲击参数在 y 方向的分量
- b = √(b_x² + b_y²)

## 4. 数值积分方法

### 4.1 哈密顿形式

使用哈密顿形式避免符号歧义（dr/dλ 的 ± 符号）：

```
H = ½ g^{μν} p_μ p_ν = 0  （光子）

运动方程：
dx^μ/dλ = ∂H/∂p_μ = g^{μν} p_ν
dp_μ/dλ = -∂H/∂x^μ = -½ (∂g^{αβ}/∂x^μ) p_α p_β
```

### 4.2 逆度规分量

在 Boyer-Lindquist 坐标中：

```
g^{tt} = -[(r² + a²)²/Δ - a² sin²θ] / Σ
g^{tφ} = -2aMr / (ΣΔ)
g^{rr} = Δ/Σ
g^{θθ} = 1/Σ
g^{φφ} = (Δ - a² sin²θ) / (ΣΔ sin²θ)
```

### 4.3 度规导数

需要计算 ∂g^{αβ}/∂r 和 ∂g^{αβ}/∂θ：

```
∂Σ/∂r = 2r
∂Σ/∂θ = -2a² sinθ cosθ
∂Δ/∂r = 2r - 2M
∂Δ/∂θ = 0
```

### 4.4 RK4 积分器

```python
def rk4_step(state, dt):
    k1_v = velocity(state)
    k1_f = force(state)
    
    k2_v = velocity(state + 0.5*dt*k1_v)
    k2_f = force(state + 0.5*dt*k1_f)
    
    k3_v = velocity(state + 0.5*dt*k2_v)
    k3_f = force(state + 0.5*dt*k2_f)
    
    k4_v = velocity(state + dt*k3_v)
    k4_f = force(state + dt*k3_f)
    
    new_pos = pos + dt/6 * (k1_v + 2*k2_v + 2*k3_v + k4_v)
    new_mom = mom + dt/6 * (k1_f + 2*k2_f + 2*k3_f + k4_f)
    return new_pos, new_mom
```

## 5. 施瓦茨柴尔德极限 (a = 0)

当 a = 0 时，Kerr 度规退化为施瓦茨柴尔德度规：

- Σ = r²
- Δ = r² - 2Mr
- q = 0（轨道在赤道面）
- R(r) = r⁴ - Δ·b²

光子加速度退化为 Binet 形式：
```
a = -1.5 h² x / r⁵
```

其中 h = r² dφ/dλ 是守恒的角动量。

## 6. 吸积盘交叉检测

在 Kerr 时空中，吸积盘位于赤道面 (θ = π/2)。检测光子是否穿过盘面：

```
检查 θ_prev 和 θ_curr 是否在 π/2 两侧
如果是，线性插值计算交叉点
检查交叉点的 r 是否在盘内 (rin ≤ r ≤ rout)
```

## 7. 参考

1. Misner, Thorne, Wheeler (1973) "Gravitation" — 经典教科书
2. Teukolsky (2015) "The Kerr Metric" — 现代综述
3. Chandrasekhar (1983) "The Mathematical Theory of Black Holes"
4. Bardeen, Press, Teukolsky (1972) "Rotating Black Holes"
5. EHT Collaboration (2019) "First M87 Event Horizon Telescope Results"