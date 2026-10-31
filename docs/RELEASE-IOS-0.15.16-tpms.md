# MotoNav iOS 0.15.16-tpms

通用圆屏固件 **7.12.11-opt.3** 配套 · 基于 `0.15.15-tpms`（冷淡科技壳 + TPMS + CoverBridge + PR1/PR2/PR5）叠 **高速模拟导航地图空白修复**。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。**

## 本版内容

- **高速地图传输**：≥80 km/h 禁用 DXR（只发 DFL / 必要时 plain RLE）；DXR 重叠门槛 ≥50%。
- **ACK 超时回退**：高速 1.2 s 重试；超时先强制 DFL；连续 2 次 NACK/超时 → 临时 plain RLE，直到再次 ACK。
- **地图优先 BLE 队列**：type 3 在飞/在队时封面（type 5）不再插队；新地图可插到封面前面。
- **迟到包策略**：已出视野的迟到包不覆盖本地仍有效的底图。
- **功能/视觉基线不变**：冷淡科技壳、四 Tab、TPMS、连接动画、导航玻璃层、PR1 BLE、PR2 MD、PR5 `#Z`、CoverBridge zlib-store 封面。

## 固件

- **请同时刷入** `7.12.11-opt.3`（音乐页布局 opt.2 + DXR 大位移 NACK / 保底图）。
- 本 Draft 工作流可能附带同分支源码构建的 panel0 zip（多为 opt.1 基线）；**实机以独立 `MotoNav-universal-firmware-7.12.11-opt.3.zip` 为准**。

## 安装

- IPA 文件名：`MotoNav-0.15.16-tpms-unsigned.ipa`（unsigned，侧载如 iLoader）；同 Bundle ID 覆盖安装，设置与配对保留。
- 高德 Key **不在包内**，首次在 App「Key 设置」自行填写。
- 校验：见 `SHA256SUMS.txt`。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
- 无个人 Rebel 开机资源或其它私有品牌素材打包。
- IPA 已 strip 调试符号路径；需接收方自行签名安装。

## 自测建议

连上圆屏 → 刷入 `7.12.11-opt.3` → 安装本 IPA → 开地图 → 模拟导航约 120 km/h 跑几分钟（可顺带放音乐看封面是否仍更新、地图是否还在）。若仍空白，反馈是否在放音乐、主题是否带建筑楼块、蓝牙日志是否有 `map-nack-timeout` / `map-md-legacy-rle`。
