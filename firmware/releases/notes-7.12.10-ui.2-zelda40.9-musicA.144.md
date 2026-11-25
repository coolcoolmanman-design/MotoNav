# musicA.144 · 高架抬升加强 + 重启原因可追溯

- 版本：`7.12.10-ui.2-zelda40.9-musicA.144`
- 基线：`musicA.143`
- 配对：iOS 0.16.2.130（无需升级）

## 1. 高架 / 桥梁抬高更明显

.142 的 14px/8px 在 466 圆屏上只占 3%，实机看不出高度。本版：

| 项 | .143 | .144 |
|---|---|---|
| 高架 / 立交 / 大桥 | 14 px | **28 px** |
| 匝道 / 中小桥 | 8 px | **16 px** |
| 桥腹侧面 | 无 | **抬升线与原地线之间填一条竖带**（夜间 0x2945 / 白天 0x9CD3） |
| 落地投影 | 宽 1.9× | 宽 2.3× 且下移 3 px |
| 护栏 | 无 | 桥面两侧各一条 1.2px 亮线（夜间 0x5D9E / 白天 0xB615） |

有「侧面」才有体积感，单纯把线往上挪只会像加粗。上桥仍是 0.55s 平滑过渡，地面段零变化。

## 2. 重启原因可追溯（诊断，不改行为）

之前「导航中重启」查不出来源，因为：

- `crumbReasonName()` 没映射 `ESP_RST_USB(11)` / `SDIO` / `EXT` / `CPU_LOCKUP`，全显示成 `other`；
- 存档条件排除了 `sw`，而固件主动重启（WiFi 预装、关快刷、改 qspi、PWR 菜单）**全是 sw** —— 正好把要查的那一类漏掉。

本版：

- 复位原因补全：`unknown / poweron / ext / sw / panic / int_wdt / task_wdt / wdt / deepsleep / brownout / sdio / usb / jtag / efuse / pwr_glitch / cpu_lockup`；
- 新增**发起者标签**（第 6 个字段）：`fastflush` / `qspi` / `wifimap` / `powerkey` / `-`；
- 除正常上电外**全部存档**到 NVS，`#RR?` 可读回；串口开机打印：

```
[CRUMB] RR,<reason>,<stage>,<frames>,<minHeap>,<ms>,<who>  (reason,stage,frames,minHeap,ms,who)
```

- WiFi 预装重启的日志补上是否在导航中：`WIFI MAP first enable: rebooting for preinit (inNav=1 navDist=350m)`。

## 3. 诊断补充

- `fastFlushInit()` 失败时打印 `[FAST] bufN alloc fail, internal free=… largest=…`（本轮实测 `fastDev=0`，快刷未启用，推屏走逐行慢路径 117ms）。
- `[CROSS] parse ok` 改为打印**全部 6 个调色板槽**：`pal=%04X,%04X,%04X,%04X,%04X,%04X`（第 5、6 个是路线色 / 箭头色，用于定位「路口图出现蓝/粉红」）。

## 4. 已知问题

- 路口图彩槽偏色是 **iOS 侧** `CrossImageSender.saturateAccent` 的扇区换算 bug（`hh = h * 60`，h 本身已是 0~6 扇区单位），需 iOS 0.16.2.131 修复，本版固件不改调色板。
