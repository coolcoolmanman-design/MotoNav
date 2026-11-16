# MotoNav iOS 0.15.34-tpms

通用圆屏固件配套 · 基于 `0.15.33-tpms`：**电子吧唧** MVP（圆屏 Tab 上传卡 + BLE bulk **type 7**）。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

### 电子吧唧
- 圆屏 Tab：在「导航屏」与「胎压监测」之间新增 **电子吧唧** 卡
- 相册选图 → **短边居中裁方** → **466×466** RGB565 → zlib → BLE bulk **type 7**（`'BDG'` 头）
- 圆裁预览与设备同裁；绿标「已推送到圆屏」/「更换图片」
- **导航 / 巡航 / 网页模拟导航中不发 type 7**；开始时 `#BM,0` 并清空队列
- 号段避开上游安卓 `TYPE_COVER = 6`；本侧音乐封面仍为 type 5

### 继承 0.15.33
- 磨砂银连接壳、圆屏卡位姿、模拟导航不记轨迹

### 固件

请配对 **`7.12.11-opt.11`**（上划进吧唧、type 7、`CAP,BDG,1`、`#BM,0/1`）。

## 安装

- IPA：`MotoNav-0.15.34-tpms-unsigned.ipa`（unsigned 侧载）；同 Bundle ID 覆盖安装。
- 高德 Key **不在包内**。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
