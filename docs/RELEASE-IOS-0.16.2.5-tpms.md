# MotoNav iOS 0.16.2.5-tpms

Draft-only · 配对固件 `7.12.10-ui.2-zelda40.9-musicA.3`。

基线：`0.16.2.4-tpms` 液态底栏；本包修 musicA.2 现场仍失败的时钟/封面。

## 本包

1. **CoverPacket 回到 128×128**（240 bulk 过大拖死 type5 / 槽位 5–6 文案）  
2. **同仓固件 musicA.3**  
   - 接受 `#TIME,<utc>,<offset>`（0.16.2.1+ PhoneClockSync）→ 时钟与秒环恢复  
   - COVER_HI 定点数慢旋；去掉整屏 chrome 缓存  
3. **版本** `MARKETING_VERSION 0.16.2.5` · `CURRENT_PROJECT_VERSION 20` · `APP_VERSION iOS v0.16.2.5-tpms`

## 刷机顺序

1. **先刷**固件 Draft `zelda40.9-musicA.3`  
2. 再装本 IPA  
