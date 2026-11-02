# MotoNav iOS 0.16.2.4-tpms

Draft-only。基线 `0.16.2.3-tpms`，配对固件 `7.12.10-ui.2-zelda40.9-musicA.2`（固件侧无改动）。

本包只做两处 UI 打磨：**底栏液态玻璃**、**连接动画设备材质**。

## 1. 底栏液态玻璃（`LiquidGlassTabDroplet.swift`）

旧实现有三处结构性缺陷，所以怎么调都到不了参考视频的效果：

| 旧实现 | 问题 |
|---|---|
| `stretch = 1.38` 然后 `asyncAfter(0.22)` 复位 | 假拉伸：与手指无关，按固定节奏缩放，看起来像"弹一下"而不是"被拖动" |
| `LiquidTabStretch`（`GeometryEffect`） | 作用在**已渲染图层**上做缩放，玻璃的折射与圆角被位图拉伸，越拉越糊 |
| `GlassEffectContainer` 内只有选中项有玻璃 | 容器里只有一块玻璃，没有可融合的对象，"黏连"无从发生 |

新实现：

- **连续位移**：`progress: CGFloat`（0…n−1）由 `DragGesture(minimumDistance: 0)` 直接驱动，玻璃实时跟手，不再整格跳
- **速度拉伸**：用 `predictedEndTranslation − translation` 预测剩余位移，拖得越快拉伸越明显（上限 1.42×），松手 spring 归位
- **真融合**：底栏本体与选中胶囊是**同一 `GlassEffectContainer` 内的两块玻璃**，选中胶囊用 `.glassEffectID("liquid-selection")` 与容器关联，重叠区域由系统融合渲染
- **高光跟随**：`LiquidBlueHighlight.flow` 由拖动位移驱动（−1…1），静止归零；去掉原来 `repeatForever` 的自转（既抢注意力又持续占用渲染）
- **点击与拖动统一**：位移 < 8pt 按落点定档，否则按 `progress` 就近吸附；一个手势同时覆盖两种交互
- 旧系统（iOS 15…25）走同一套手势与吸附，玻璃降级 `LiquidGlassDroplet`

## 2. 连接动画设备材质（`ConnectPuckSceneView.swift`）

塑料感的根因不是几何，是材质与光照：

| 旧值 | 为什么像塑料 |
|---|---|
| `metalness 0.68` | 金属度不满 = 混入 32% 漫反射，这正是塑料的物理特征 |
| `roughness 0.58` + 亮银 diffuse | 高粗糙 + 亮白 = 磨砂塑料 |
| 6 面 64×64 均匀灰立方体环境 | 金属反射没有对比，读成一块灰 |
| `normal.intensity 0` | 颗粒只写在 diffuse 上，不参与高光，反射仍然平滑 |

新值：

- `metalness 1.0` / `roughness 0.42` / `fresnelExponent 3`，diffuse 压暗到冷灰 `(0.60,0.63,0.68)`
- **大面柔光板环境**（程序化 1024×512 等距柱状图，4 块径向柔光）——喷砂是面光源质感，细灯条会在砂面上拉出不该有的条状高光
- **随机颗粒法线**（256²，噪声高度场 → 盒式平滑 → 梯度转法线，7×7 平铺）
- 圆角统一 **R0.15**：圆柱侧面 + 上下 `SCNTorus` 与侧面/端面相切，外沿连续，不再有直角高光断线；因圆角必须 < 半厚/2，半厚 0.145 → 0.165
- 端面随圆角内收：黑边 0.93 → 0.845，屏幕 0.87 → 0.83
- 屏幕加 PBR 半透玻璃盖板（`transparency 0.20` / `roughness 0.05`），出真实环境反射
- 灯光压暗以配合 IBL：ambient 180→95、key 720→500、fill 260→200、rim 380→300
- 侧键贴边、5 孔退到壳面内侧（原来黑球凸在表面，读成"凸点"而非孔）

## 待真机验证

1. 底栏按住拖动：是否实时跟手、拉伸是否够"液"
2. 底栏玻璃与选中胶囊在 `GlassEffectContainer` 内是否按预期融合（`Fusion` 行为依赖系统渲染，需真机确认）
3. 连接弹层设备：喷砂颗粒在真实光照下是否可见、整体明暗是否合适
