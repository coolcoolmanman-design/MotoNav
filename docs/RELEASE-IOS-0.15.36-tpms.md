# MotoNav iOS 0.15.36-tpms

通用圆屏固件配套 · 基于 `0.15.35-tpms`：**取消吧唧轮播**、修复上传空白、连接磨砂银实装、底栏液态玻璃可见点按。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

### A · 电子吧唧
- **取消多图轮播 UI**：仅单图
- **分钮**：「选择图片」≠「上传图片」（选图不自动推送）
- 上传发 **ver=1 单包** type 7（兼容已刷 **opt.12**；避开多包 `replace` 丢包）
- 识别 `CAP,BDG,1` / `CAP,BDG,2` 后可补推
- 裁切：自动居中；自选区域 **自由宽高比** → 选区内居中裁方 → **466×466**

### B · 连接动画
- SceneKit 磨砂银：IBL + BrushedMetal 颗粒、去 multiply 压平、环境光回升
- 侧格栅/按键/边缘 fillet 保留
- 专用 `ConnectGroundShadow` 软椭圆影（与圆屏 Tab 紧影分离）

### C · 底栏
- 高对比冷银青透镜 overlay + ~280ms linger
- 底栏移出 `.id(page)`，切 Tab 不再撕掉点按动画
- 切 Tab 折射高光加强（无紫晕）

### 固件

请配对已发布 Draft **`7.12.11-opt.12`**（无需 opt.13）：  
https://github.com/coolcoolmanman-design/MotoNav/releases/tag/untagged-55c7210069bf1c706c73  
zip：`MotoNav-universal-firmware-7.12.11-opt.12.zip`

## 安装

- IPA：`MotoNav-0.15.36-tpms-unsigned.ipa`（unsigned 侧载）；同 Bundle ID 覆盖安装。
- 吧唧：选图 →（可选裁切）→ **上传图片** → 设备主屏 **上划**。
- 高德 Key **不在包内**。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
