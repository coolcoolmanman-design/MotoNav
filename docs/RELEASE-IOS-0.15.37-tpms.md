# MotoNav iOS 0.15.37-tpms

基于 `0.15.36-tpms`：**底栏苹果液态玻璃液滴** + **连接圆盘磨砂银重建**。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。无新固件。**

## 本版内容

### A · 底栏液态玻璃液滴
- 选中态为可横向滑动的**玻璃球体/液滴**（`matchedGeometryEffect`）
- 色散边缘（青–品红–黄）、顶部高光、底部棱镜焦散
- 液滴下图标染成鲜明系统蓝；底栏毛玻璃，液滴更通透
- 对齐参考：`media/ios-0.15.37-glass-puck/liquid-glass-ref.jpg`

### B · 连接圆盘重建
- **磨砂银** PBR（颗粒贴图 + 中金属度/高粗糙度 + 六面环境），禁白塑料高光
- 强边缘 fillet（粗 torus）+ 侧 5 孔格栅 + 双实体键
- 软椭圆地面影挂在 **scene root**（不随 puck tilt/自旋）——修复 0.15.36 平行四边形方影
- 屏贴仍为 mockup C（200 m / 28 km/h）

### 0.15.36 白塑 + 方影根因
1. 阴影节点挂在倾斜/自旋的 puck 子树 → 地面平面跟着 tilt → 视口平行四边形/方影  
2. PBR 环境仅 `UIColor`（非 cubemap）+ 偏高 ambient/metalness → 洗成白塑料

### 固件
配对既有 **`7.12.11-opt.12`**（不变）。

## 安装
- IPA：`MotoNav-0.15.37-tpms-unsigned.ipa`（unsigned 侧载）
- 高德 Key **不在包内**
