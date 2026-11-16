# MotoNav 固件 7.12.10-ui.2-zelda40.9-musicA.35

配套 iOS：**0.16.2.32**（建议一起升级，见文末兼容性）

## 修掉「相邻很近的两个路口，第二张放大图不显示」

实机反馈：连续两个近路口时，第二个路口的放大图来不及推到圆屏就不显示。
查下来主要不是 BLE 传输慢（预览只有 5 片、0.4~0.6 秒，本来够用），
而是两层防抖逻辑把新图误杀了：

1. **固件 2 秒静默窗（决定性）**：`#CXH` 会置 `crossHideAt`，`parseCross()` 开头
   只要 `millis() - crossHideAt < 2000` 就无条件丢整包。这个窗是 musicA.29
   为防「过了路口又闪回来」加的，但分不清**上一个路口的迟到残包**和
   **下一个路口的新包**。第二张预览 0.4~0.6 秒到达正好撞进去，
   串口会打 `[CROSS] 已过路口，丢弃迟到整包 ...B`。
2. **App `hide()` 太粗暴**：无条件 `cancelBulk(9/10)` 再发 `#CXH`。
   高德连续路口的回调可能是 `show(新图) → hide(旧图)`，新图刚入队就被清掉。

### 本版改法：静默窗按「代」精确丢

`bulk` 头里的 `id` 每发一张路口图 +1，预览(10) 和全图(9) 共用同一个 id，
正好当作「代」，**不用改协议、不用改命令格式、不用改尺寸**：

- `MapState.h` 新增 `lastCrossGen`（最近成功上屏的一代）和 `crossHiddenGen`（被隐藏的一代）。
- `parseCross()` 成功时记 `lastCrossGen = gen`；BulkReceiver 把 `d[2]` 的 id 传进来。
- `#CXH` 时 `crossHiddenGen = lastCrossGen`，只静默这一代。
- 静默窗内：
  - `gen == crossHiddenGen` → 同代迟到残包，丢弃（保留 musicA.29 原有的防闪回能力）；
  - `gen != crossHiddenGen` → **新一代到了，立即解禁**，并打
    `[CROSS] 新一代路口图 gen=X（隐藏的是 Y），解除静默`，后续全图也照常收。

`crossHiddenGen < 0`（本次上电还没成功显示过图）时退回旧行为：静默期内全丢。

## 串口怎么看

```
[CROSS] parse ok preview 136x75 ...          ← 第一个路口
[CROSS] hide                                 ← 通过
[CROSS] 新一代路口图 gen=7（隐藏的是 6），解除静默
[CROSS] parse ok preview 136x75 ...          ← 第二个路口，正常上屏
[CROSS] parse ok full 272x150 ...
```

若仍看到大量 `[CROSS] 已过路口，丢弃同代迟到整包 gen=...`，说明丢的是上一代残包，属正常。

## 兼容性

- **`#CXH` 命令格式未变**，二进制 bulk 协议、`CROSS_W/H/PW/PH`（272×150 / 136×75）、
  `ver=2` 5 色调色板全部未动。
- 新固件 + 旧 App：兼容，静默窗按代工作（旧 App 的 bulk id 同样递增）。
- 旧固件 + 新 App：仍会隐藏，只是没有「按代精确放行」；建议两端一起升级才拿到完整效果。
- 车道胶囊（musicA.34 的 `CROSS_LANE_Y=224`）等布局不变。
