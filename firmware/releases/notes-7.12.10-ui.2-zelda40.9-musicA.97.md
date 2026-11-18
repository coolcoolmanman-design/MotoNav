# MotoNav 固件 musicA.97 · SoftAP 打开修复（WiFi 地图加速）

> 配套 iOS **0.16.2.87**。基线 musicA.96。**musicA.84 仍不安全，勿刷。** Draft 保持。

## ⚠️ 分区表（musicA.95 起仍是 4MB app）

刷机须先写分区表再写 app（与 musicA.95/96 相同）：

```
esptool --chip esp32s3 --port COM5 --baud 921600 write_flash \
  0x8000  MotoNav_universal_1.75_v7.12.10-ui.2-zelda40.9-musicA.97_panel0_partitions_0x8000.bin \
  0x10000 MotoNav_universal_1.75_v7.12.10-ui.2-zelda40.9-musicA.97_panel0_app_0x10000.bin
```

## 本版

- **修无法打开圆屏热点**：NimBLE 已连接时 `WiFi.softAP()` 常首次失败。
  - 先 `WIFI_OFF` 再 `WIFI_AP`，`softAPConfig(192.168.4.1)`，保持 `WiFi.setSleep(true)`（共存）。
  - 信道 1/6 交替，`max_connection=2`。
  - 主循环最多再试 4 次（合计 5）；**用尽才**回 `WIFI,0,err`，避免 App 把「打开中」当成永久失败。
- 其余同 musicA.96（非阻塞 TCP 收图、申花雷电开机、4MB 分区）。

## 校验

- 开机 banner：`MotoNav ready, firmware v7.12.10-ui.2-zelda40.9-musicA.97`。
- app.bin 上限 4,194,304 B。
