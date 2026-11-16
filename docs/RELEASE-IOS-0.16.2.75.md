# iOS 0.16.2.75 · musicA.81

基线 **0.16.2.74** / **musicA.80**（`ae938d7` / Drafts `untagged-0525e098be07e6730226` + `untagged-ee1b349dad2a357c4564`）。
Draft 不 publish；资产脱敏。

## 本包内容

**申花中央动画 v2（David 确认效果图后落地）**

- 未连接 idle：清晰拳击循环，无盔（v2 帧）。
- 已连接 connected：蓝骑手无盔骑摩托 + 红小人趴地拖绳同帧（朝向锁定）。
- Boot 沿用 v1；等红灯微远推沿用 musicA.80（本轮不改）。
- 源：`media/shenhua-anim-mockup-v2/{idle,connected}/`；bake `scripts/bake-shenhua-frames.py --center-only`。

## 版本

- iOS `MARKETING_VERSION` → `0.16.2.75` · build `84`
- FW `7.12.10-ui.2-zelda40.9-musicA.81`

## 建议实机

双端同刷：开机申花仍为 v1；中央「申花」测 idle 对打与 connected 趴地拖绳。
