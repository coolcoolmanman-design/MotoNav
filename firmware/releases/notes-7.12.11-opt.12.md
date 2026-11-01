# 7.12.11-opt.12

Stack: opt.11 tip + **电子吧唧多图轮播**。

## Changes

- **Bulk type 7 ver=2**：`'BDG' 2 slot count w_le16 h_le16 + zlib(RGB565)`（头 10B）；最多 **3** 槽；原生 **466×466**。
- **兼容 ver=1**：单图写入 slot 0（opt.11 客户端仍可用）。
- **手势**：吧唧页 **左右滑** 切图；**下划** 回主屏；主屏上划进入不变。
- **自动轮播**：多图时约 3s 切下一张；底部点位。
- **能力**：`CAP,BDG,2`；ACK 仍 `K,7,id`；`#BM,0/1` 互斥不变。
- type 6 仍不接收（上游 TYPE_COVER）。

## Pairing

Flash with iOS **0.15.35-tpms**. Prefer app @ `0x10000`.
