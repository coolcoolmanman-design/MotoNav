# MotoNav iOS 0.16.2.7-tpms

Draft-only · 配对固件 `7.12.10-ui.2-zelda40.9-musicA.5`（也可留在 musicA.3/4，二者无协议耦合）。

基线：`0.16.2.6-tpms`。本包两项：底栏液态玻璃走路径 A 完全体 + 连接设备模型按 v4 效果图（用户实物照片校准、Three.js 预览逐参数验证）移植。

## 本包

1. **底栏：iOS 26 原生液态玻璃完全体（路径 A）**
   - 底栏本体进入 `GlassEffectContainer`（原生 `.glassEffect`），与选中透镜**同容器融合**——靠近时由系统合成器融合，即参考视频的核心观感；0.16.2.6 底栏还是 `ultraThinMaterial`，与透镜不融合
   - 标签仍在玻璃上层（chrome 在 HStack 的 background 层），不复现 0.16.2.4 的"玻璃吞标签"
   - 竖栏（Duo Rail）同步：黑色半透明底换成原生玻璃 + 融合
   - iOS 15–25 兜底不变：`LiquidGlassDroplet` 手搓玻璃 + 同套拖动手势
2. **连接设备模型按 v4 效果图移植（SceneKit）**
   - 机身增厚：半厚 0.165→**0.19**、圆角 0.15→**0.095**（对照实物照片的厚度与圆润过渡）
   - 侧键改**壁面内嵌**：暗底槽（#30353c）+ 银色键芯沿轮缘切向贴壁，上长（0.30/0.34）下短（0.14/0.18），方位角 ±0.7~0.8rad；不再是悬在端面外的竖向凸钉
   - 5 麦克孔改沉孔圆柱，外端面与壁面齐平（修穿模）
   - 材质对齐 v4：diffuse #e0e4ea / metalness 0.78 / roughness 0.44 + 喷砂颗粒法线
   - 屏幕贴图按实物 UI 重绘：转向徽章、200 m·即将左转、白芯蓝路线 + 导航箭头、28 km/h 白圆（1024² 程序化绘制，坐标即 v4 Canvas 原值）
   - 法线方向教训（v4 效果图踩坑）：Lathe 轮廓点顺序决定法线朝向；SceneKit 侧用 Cylinder+Torus+Cap 组合几何，法线由 SceneKit 保证朝外，无此问题
3. **版本** `MARKETING_VERSION 0.16.2.7` · `CURRENT_PROJECT_VERSION 22` · `APP_VERSION iOS v0.16.2.7-tpms`

## 安装

同 Bundle ID 覆盖安装 `0.16.2.6-tpms`；固件 musicA.5 已刷（COM5，NVS 未擦），无需重刷。
