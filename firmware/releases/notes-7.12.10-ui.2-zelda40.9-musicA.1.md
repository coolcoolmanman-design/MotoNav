# MotoNav 固件 7.12.10-ui.2-zelda40.9-musicA.1

Draft-only · 通用圆屏 1.75 panel0 · 基线 `7.12.10-ui.2-zelda40.8-mapfix.1`。

## 本包

1. **音乐页方案 A 冷雾银**（用户确认 mockup）  
   - 主页左划 → 音乐控制（手势槽不变）  
   - 满屏封面绕圆心慢旋（播放中；暂停停转）；歌名/歌手/进度/三键为静止 overlay  
   - 去掉「MUSIC」/「音乐」标题  
   - 三键透明液态/磨砂玻璃近似  
   - 色：标题 `#E8EEF4`、歌手 `#BAC6D2`、时长 `#A0B2C0`、进度 `#91C3D7`；底区轻 scrim  

2. **恢复音乐封面 bulk type 5**（`CVR` / 128×128，与 iOS `CoverBridge` 对齐；mapfix 线曾空留 type5）  
3. **#Z 时长字段**（可选 duration/elapsed）供本地进度条  

## 刻意保留

- 地图 mapfix / phone.8 管线、电子吧唧 type7、TPMS、右滑吧唧  

## 配对

- iOS Draft `0.16.2.2-tpms`（同方案 A；CoverBridge type5）  

## 刷机

- 优先 `app_0x10000.bin` @ `0x10000`（保留 NVS）  
- 全新机用 `full_0x0.bin`  
