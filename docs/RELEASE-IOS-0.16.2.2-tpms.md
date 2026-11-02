# MotoNav iOS 0.16.2.2-tpms

Draft-only · 配对固件 `7.12.10-ui.2-zelda40.9-musicA.1`。

基线：`0.16.2.1-tpms` 地图 Build16 管线 + 液态玻璃底栏；本包叠 **音乐页方案 A**。

## 本包

1. **圆屏音乐页方案 A 冷雾银**（用户确认 mockup）  
   - 设备主页左划 → 满屏慢旋封面 + 静止 overlay（歌名/歌手/进度/玻璃三键）  
   - 去掉「MUSIC」标题；色 `#E8EEF4` / `#BAC6D2` / `#A0B2C0` / `#91C3D7`  
   - 固件恢复 bulk **type 5** `CVR`；本包 `CoverBridge` 仍推 128×128（协议不变，设备端满圆上采样慢旋）

2. **版本** `MARKETING_VERSION 0.16.2.2` · `CURRENT_PROJECT_VERSION 18` · `APP_VERSION iOS v0.16.2.2-tpms`

## 刻意保留

- 0.16.2.1 地图修复、吧唧 type7、TPMS、液态玻璃底栏、非机动车文案  
- **不改**连接动画  

## 刷机 / 安装顺序

1. 先刷固件 Draft `zelda40.9-musicA.1`  
2. 再装本 IPA  
EOF