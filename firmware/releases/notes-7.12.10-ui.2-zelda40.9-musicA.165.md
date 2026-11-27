# musicA.165

只加诊断，不改版式与协议：给原生 3D 底图（P3D，bulk type 13）补串口日志。

## 为什么

iOS 0.16.2.137 装上了、开关也开了，圆屏仍显示瓦片，但固件侧**完全看不到**是「App 一帧都没发」还是「发了被拒」还是「收到了但朝向过期」——三种情况外观一样。

## 加了什么

`src/map/Persp3D.h`，全部以 `[P3D]` 开头，只在原因变化或每 8s 打一次（不会刷屏）：

1. **收到整帧**
   `[P3D] rx #12 448x448 lut=64 d=90..2680m brg=214.5 pitch=46.0 31256B pal=1`
   —— 有这行说明 BLE 通道、记录边界、游程解码全都通了，剩下的只是朝向匹配。

2. **解析失败原因**
   `[P3D] reject: magic / size / lutn / dist / pal-short / lut-short / too-big / nomem / pixels-incomplete (len=…)`
   —— `pixels-incomplete` 表示游程没铺满 w×h（多半被截断，看 `len` 是否明显小于上一帧）；`magic` 说明包根本不是 P3D（App 侧类型号串了）。

3. **每帧可用性**（`p3dDrawRows` 每帧调用一次）
   `[P3D] ok        use=… skip=… frame=12 age=3200ms brg=214.5 err=3.2 w=448 lut=64`
   `[P3D] no-frame  …`（0.16.2.137 之后一帧都没收到，或都在解码阶段被拒）
   `[P3D] stale-bearing … err=27.4`（帧的朝向与当前车头差 > 8°，正在回退旧 `rasterRows3D`）
   —— `3d-off` 不打日志：3D 关卡关着时 P3D 本来就用不上。

## 诊断口径

| 串口现象 | 结论 |
|---|---|
| 一直没有 `[P3D]` 行 | 3D 关卡没开，或主题不是 15/16（P3D 根本不参与） |
| `no-frame` | App 侧没发：查 iOS「现在发不出去：…」提示 |
| `reject: pixels-incomplete` | 包被截断（BLE 单包上限 / 重发没到齐） |
| `stale-bearing err>8` | 收到了但车头已转走：等 App 下一帧，或停车时手动「立即传一帧」 |
| `ok` 但看着还是瓦片 | 说明 P3D 已在画，问题在 LUT 距离/画质，不再是链路 |

## 兼容

协议、bulk 类型、NVS、版式、绘制路径全部不变；日志走 `Serial.printf`，与 `[CROSS]` / `[FPSSTAT]` 同一条串口输出。
