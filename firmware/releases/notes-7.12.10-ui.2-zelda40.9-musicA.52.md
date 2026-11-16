# 固件 7.12.10-ui.2-zelda40.9-musicA.52

配套 iOS：`0.16.2.49`。基线 musicA.51（`10f1af9`）。

## 一、中央槽「组合」精灵（vehId=4）

- 沿用 musicA.50 CSP 通路：bulk type 11、`parseCsp`、ffat `/center/{sleep,chase}.csp`、chroma 0 叠画。
- App 改为 **性别 × 八车型预制** 推送两态循环；固件侧无上传逻辑变更。
- `#FV` 注释：4 = **组合**（原「自定义」）；2 = 自行车仍接受但 **App 下拉已隐藏**（`drawBikeRider` 保留）。

## 二、地图 / 其它

- musicA.51 矢量简图（id 13）与既有 GTA/塞尔达/电量管理行为不变。
