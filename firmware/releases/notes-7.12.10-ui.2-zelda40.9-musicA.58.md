# 固件 7.12.10-ui.2-zelda40.9-musicA.58

基线 musicA.57 / iOS 0.16.2.55（`c47b70c` · Draft
`untagged-abe557dda1a189e79ced`）。配套 iOS **0.16.2.56**。

## C Infograph（全屏指针 · `fullFaceAB=2`）

方案 1：type8 烘焙底图 + 固件叠指针/数据（确认稿 07v2）。

- `#FC3` 第四字段钳到 **0..2**（0 A 纯净 / 1 B 外环 / 2 C Infograph）
- Infograph 路径：跳过 `csAnalog` 程序面盘与 B 实心功能圈；在壁纸上画
  - 外四弧轨内彩进度（天气 / UVI / 电量 / AQI；半宽 2.4）
  - 天气两端最低/最高 + 弧正中轨外侧当前温
  - 电量弧外侧 `%`；AQI 读数在白盘内侧；UVI 暂 `--`（协议无 UVI 字段）
  - N/E/S/W 子盘内 SPD / LIVE G / TPMS / WX 点阵字
  - 针心上方实心电量%；诚实扁针 r=172 · 0.52/0.78/0.90 · 宽 10/7/2

## 兼容

- A/B 行为不变；未选 C 时不依赖 Infograph 壁纸
- NVS / `#FC3` schema=2 末字段不变；旧 App 发 ab=0/1 仍可

## 建议实机验证

- 下发 Infograph 底图后切 C：四弧/子盘/扁针是否对齐确认稿
- 切回 A/B：程序面盘与 B 四圈是否正常
- 无天气 / 无 AQI / 无 UVI 时读数占位是否为 `--`
