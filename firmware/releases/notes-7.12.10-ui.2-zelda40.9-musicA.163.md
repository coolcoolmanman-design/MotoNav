# MotoNav firmware musicA.163

配套 iOS：**0.16.2.136**（本版固件侧先行；iOS 侧出图在下一版接）

## 目标

把「用固件逐像素挤楼块」换成「高德渲染 3D 鸟瞰底图，固件只做轻量重投影」。
本版只落**固件侧**：新增 bulk type 13（P3D）接收、解码、重投影，以及不可用时的完整回退。
没有收到 P3D 帧时，地图表现与 musicA.162 完全一致。

## 新增

| 文件 | 内容 |
|---|---|
| `src/map/Persp3D.h` | P3D 状态、`parseP3D()` 解码、`p3dDrawRows()` 重投影、`p3dUsable()` 判定 |
| `src/protocol/BulkReceiver.h` | 接受 type 13 并分发到 `parseP3D()` |
| `src/map/MapState.h` | `bulks[14]` + `P3D_MAX` |
| `src/map/VectorMapReceiver.h` | 开机为 type 13 分配 420 KB PSRAM 缓冲 |
| `src/map/RasterRenderer.h` | `drawRasterMap()` 先试 P3D，成功则跳过 `t3dPrepare/rasterRows3D` |
| `src/map/MapFrame.h` | 没有普通瓦片但有 P3D 时也进底图绘制；P3D 生效时跳过矢量要素 |
| `docs/persp-3d-protocol.md` | 协议与采样公式、带宽评估 |

## 关键点

- **仍然要重投影**：透视图与相机位置绑定，纯贴图只能用于静止场景。固件按行查 LUT 把画布重新映射，
  没有楼块检测、没有楼高常量，也没有逐像素透视除法；导航线/车标与底图共用同一套 `u` 映射，天然对齐。
- **朝向容差 8°**：画布只对应一个 `bearing`，超出就回退旧路径，不会歪着画楼。
- **回退安全**：解析失败 / 未收到 / 朝向过期 → 与 .162 行为一致。

## 刷机

```
esptool --chip esp32s3 --port COM5 --baud 921600 --before default_reset --after watchdog_reset \
  write_flash 0x8000 <partitions_0x8000.bin> 0x10000 <app.bin>
```

**不要**写 `full_0x0.bin`。
