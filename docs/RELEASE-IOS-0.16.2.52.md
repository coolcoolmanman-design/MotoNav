# iOS 0.16.2.52

配套固件：`7.12.10-ui.2-zelda40.9-musicA.54`。基线 0.16.2.51（组合预览方向修正稿）。

## 1. 时钟样式：6 数字 / 3 指针 / A·B

- 设置页「时间样式」下拉只列 0..5（立体银白大数字 / 荧光七段 / 线框纵向 / LED 点阵 /
  斜体粗体+蓝线 / 潮流纵向双色）；「表盘样式」下拉只列 6..8（石英三针 / 运动三针 / 罗马时标）。
- A 纯净 / B 外环二级开关保留，3×2 组合；切入全屏表盘自动钳到 6..8，切回中央时间自动钳到 0..5。
- 固件 `#FC3` 回执新增 `schema=2`；App `slotModesReply()` 检测该字段后按新语义做**一次性**
  样式重置（旧 st 5..8 在新版含义不同），重置标记存 `clock_schema2_migrated`，不会反复重置。

## 2. 组合动画帧真正生效 + 预览/圆屏正立

- **方向**：预览与推送共用同一份 RGB565，两端被观察到同时倒置且解释方向一致 → 在唯一生成出口
  `build()` 对每帧做一次垂直行序反转（`flipY`），两端同时变正。不再依赖 SwiftUI 视图旋转，
  也不动固件 blit（固件逐行直拷、与 GTA 帧共用行序契约）。
- **双态送达**：`BleBulkWriteQueue.replace` 增 `append` 参数，`BleLink.sendBulk` 透传；
  `pushFrames()` 发 chase 时 `append: true`，保留 sleep 未发分片。原来两包同 type 11 背靠背发送，
  第二包按 type 清空队列会删掉第一包全部分片 → 设备只装 chase，双态切换从未可见。
- **帧内容**：sleep / chase 的 6 帧车身起伏从 0/1px 提到 0..9px；
  8/16 组合的 sleep 与 wave 烤帧字节相同时，给未连接态整体压暗 72%，两态不再同图。
- **旧缓存**：`noteDeviceCached()` 在 `csp_frames_v2_pushed` 未置位且可发送时强制重推一次，
  覆盖 ffat 里 0.16.2.50 落盘的旧（倒）帧，避免 `CSP,cached,1` 让修复永远看不到。
- 固件侧配套：两态都装好后按 4 秒周期交替显示（详见固件发布说明）。

## 3. 其它

- `clockStyleNames` 9 个名称同步更新；`RELEASE_VERSION` / `MARKETING_VERSION` /
  `CURRENT_PROJECT_VERSION (62)` 已同步；CI 白名单加入 `cursor/fix4-clock-vectorsprite`。
- 全部 Swift 文件保持 CRLF；`git -c core.whitespace=cr-at-eol diff --check` 通过。
