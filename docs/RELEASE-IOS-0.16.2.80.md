# iOS 0.16.2.80 · musicA.86

基线 **0.16.2.79** / **musicA.85**（`8de00e3`）。Draft 不 publish。

## 紧急提醒

**musicA.84 仍不安全，勿刷**（超分区砖机）。解砖 / 安全基线为 **musicA.85**。

## 本包（申花中央实机反馈）

1. **断连态**：断开后中央须稳定 idle（对打）；connected 仅在 `shownConn` 真连接时。修 iOS `#OFF` 队尾清 `appBye` 竞态 + FW 保活不解除离线；WAIT_LOST 不再冻住 connected 残帧。
2. **抠图**：软 alpha key（≥36 + 1px dilate），去掉导引线；避免衣物/身体挖空。
3. **尺寸**：中央存 **189×135**，2× ≈ **378×270**（约 90% 相对 musicA.85 的 420×300）。
4. 开机 AA / 等红灯未改。
5. Flash：本地 `app.bin` **3133392**（余量 **12336 B** ≫ 1.5 KiB）。musicA.84 仍勿刷。

## 版本

- iOS `0.16.2.80` · build `89`
- FW `7.12.10-ui.2-zelda40.9-musicA.86`
