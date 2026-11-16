# 固件 7.12.10-ui.2-zelda40.9-musicA.50

配套 iOS：`0.16.2.47`。基线 musicA.49-ui.6（`ec6d751`）。

## 一、中央槽自定义精灵（bulk type 11）

- `#FV` 扩展 `0..4`（4=自定义）；开机 NVS `% 5`
- `BulkReceiver` 放宽 type≤11；`bulks[12]`；`parseCsp` ver=2 多帧 zlib
- ffat：`/center/sleep.csp` + `/center/chase.csp`；认证 `CSP,cached,1` 先于 `CAP,CSP,1`
- `drawCustomCenter` 复用 `gtaScratch` + chroma 0；inflate 经 `BootAnimation::inflateZlibRgb565`（heap，勿 tinfl 栈）
- 无图时 `vehId=4` 不置顶绘制（避免黑带）

## 二、电量管理 / 省电

- `#LP,<0|1>` 省电：降亮 cap 80、关 dynBg、中央动画冻帧（GTA/自定义/载具共用 `animNow`）
- `#LPA,<0|1>` ≤10% 自动开（默认开）；`batPoll` 插电/充电自动退
- `#PBO,<0|1>` 电池短按息屏（默认关；USB 短按始终可用）
- 低电时 `V` 上报约 15s 一次

## 三、刷机

- 建议 `*_app_0x10000.bin`（保留 NVS）
- Draft 保持隐藏；路径脱敏 `-ffile-prefix-map`
