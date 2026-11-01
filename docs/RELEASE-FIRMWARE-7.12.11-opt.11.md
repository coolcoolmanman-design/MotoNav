# 7.12.11-opt.11

Stack: opt.10 tip + **电子吧唧** MVP（静图 bulk **type 7**）。

## Changes

- **Bulk type 7**：`'BDG' ver w_le16 h_le16 + zlib(RGB565)`；原生 **466×466** 全屏缓冲；ACK `K,7,id`；能力 `CAP,BDG,1`。
- **号段**：避开上游安卓 `CoverPacket.TYPE_COVER = 6`；本侧音乐封面仍为 **type 5**（`CVR`）。type 6 本固件不接收。
- **手势**：主屏（`page==0` 且 `!inNav && !cruise`）**上划**进入吧唧页；吧唧页**下划**回主屏。
- **互斥**：导航指令 / `#z,1` 巡航 / `#BM,0` **强制退出**吧唧；导航中不可进。`#BM,1` 可进（同互斥条件）。
- 空态暗色占位；有图则整屏 blit。无玩具 / GIF / 视频。

## Pairing

Flash with iOS **0.15.34-tpms**（圆屏 Tab「电子吧唧」上传卡，发 type 7）。Prefer app @ `0x10000`.

## Not in this pack

PR open/merge, toys/GIF/video/AI, Wi‑Fi transfer, CoverBridge 240/320 reuse, upstream TYPE_COVER=6 ingest, upstream merge, undraft release.
