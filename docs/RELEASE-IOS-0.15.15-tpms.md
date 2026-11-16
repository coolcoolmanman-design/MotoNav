# MotoNav iOS 0.15.15-tpms

通用圆屏固件 **7.12.11-opt.1** 配套 · 基于 `0.15.14-tpms`（TPMS + 连接玻璃 + PR1/PR2/PR5 + CoverBridge）叠 **冷淡科技 UI 重绘**。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。**

## 本版内容

- **冷淡科技壳（Light + Dark）**：中性灰阶 + 单一系统蓝（`#007AFF` / `#0A84FF`）；去掉粉/青 Vice City 双强调与霓虹 HUD。
- **组件**：AppTheme / 液态玻璃 / Tab 条 / GlassButton·GlassChip / 导航转向卡与长按退出 / 圆屏 TPMS 卡 对齐冷淡科技 tokens。
- **四 Tab**：导航 / 圆屏 / 轨迹 / 设置（收藏与骑行记录保留在轨迹 Tab；圆屏设置与 TPMS 仍在圆屏）。
- **功能基线不变**：TPMS、连接动画、导航玻璃层（转向卡 + 底控 + 长按退出）、PR1 BLE、PR2 MD、PR5 `#Z`、CoverBridge zlib-store 封面。

## 固件

- 版本：`7.12.11-opt.1`
- 资产：随包 `MotoNav-universal-firmware-*.zip`（目录结构同既有通用圆屏包）

## 安装

- unsigned IPA，侧载（如 iLoader）；同 Bundle ID 覆盖安装，设置与配对保留。
- 高德 Key **不在包内**，首次在 App「Key 设置」自行填写。
- 校验：见 `SHA256SUMS.txt`。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
- 无个人 Rebel 开机资源或其它私有品牌素材打包。
- IPA 已 strip 调试符号路径；需接收方自行签名安装。

## 已知说明

- Draft 仅供自测；请在实机配对 `7.12.11-opt.1` 后验证 UI 与协议/封面。
- 连接动画电量需圆屏发 `V,pct,...`；旧固件显示 "—"。
