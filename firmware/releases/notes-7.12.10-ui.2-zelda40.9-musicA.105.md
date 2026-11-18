# MotoNav 固件 musicA.105 · 全屏转动下半撕裂修复

> 配套 iOS **0.16.2.96**。基线 musicA.104。Draft-only。

## 根因

Style 5 脏帧把 **scrim（叠在旧角封面上的半透暗层）** 打进 `musicChrome`；旋帧只转封面再 blit chrome，下半 `MUSIC_CHROME_Y0≈220` 一带仍是**冻结旧角封面+scrim**，与上半新角封面错位 → 目测下半撕裂。  
原版·冷雾银一直是「每旋帧重画 scrim，chrome 只含 overlay」。

## 修复

- 脏帧：封面 → **scrim** → 快照底 → 捕获 chrome（仅歌名/歌手/胶囊）→ 进度  
- 旋帧：封面 → **重画 scrim** → blit chrome → 进度  
- 保留 musicA.104：60ms≈16.7fps 稳态、进度局部、禁 500ms 全量 dirty

## 刷机

1. 分区表 `0x8000` → app `0x10000` 刷 musicA.105  
2. 装 IPA **0.16.2.96**  
3. 「全屏转动封面」播放中看下半是否仍撕 / 是否仍稳顺
