# MotoNav iOS 0.15.14-tpms

通用圆屏固件 **7.12.11-opt.1** 配套 · 基于 `0.15.12-tpms`（胎压 + 连接玻璃）叠 PR1/PR2/PR5 协议同步与封面修复。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入。**

## 本版内容

- **TPMS + 连接玻璃**：保留 0.15.12-tpms 胎压卡与连接动画。
- **PR1 BLE**：图文 `contentEquals` 跳过；bulk 写超时约 1s 自愈（不再 5s 断连）。
- **PR2 地图**：`CAP,MD,1` 时发送 DFL / DXR；旧固件仍走 RLE。
- **PR5 音乐**：`#Z` 可选 `duration_ms,elapsed_ms`（本地进度条）；切歌/seek 重同步。
- **专辑封面（bulk type 5）**：回移植 `CoverBridge`，并修复实测不生效（见下）。

### 封面实测不生效 — 根因与修复

固件 `parseCover` 有 `len < 6 + w*h*2` 门槛，Deflate 压缩后偏小会被直接丢弃。本版手机改发 **zlib store**（仍合法 zlib，满足长度门槛）。另：AMS 显示名带 `…` 不再用于搜图；type 5 插队避免被地图 bulk 饿死；重连补发封面。

固件侧后续仍建议放宽该长度判断，以便改回 Deflate 省流量。

## 固件

- 版本：`7.12.11-opt.1`（地图 Deflate/XOR + 极简音乐进度 + cover type5）
- 资产：`MotoNav-universal-firmware-7.12.0.zip`（目录结构同既有通用圆屏包）

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

- Draft 仅供自测；协议与封面请在实机配对 `7.12.11-opt.1` 后验证。
- 连接动画电量需圆屏发 `V,pct,...`；旧固件显示 "—"。
