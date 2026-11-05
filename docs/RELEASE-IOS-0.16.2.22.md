# iOS 0.16.2.22（路口放大图验证版）

这一版只做**验证**，不改圆屏显示：目标是把高德的路口放大图拿到手里，看清它的尺寸、比例、透明通道和体积，再决定怎么压到圆屏上。

## 新增

1. **接高德路口放大图回调**
   - `driveManager:showCrossImage:` / `driveManagerHideCrossImage:`（`AMapNaviDriveDataRepresentable`）
   - 导航中经过路口时，高德会把已渲染好的位图（宽:高 = 25:16）交给 App
2. **导航页顶部「路口图预览」卡**
   - 显示原图 + 一行元数据：`#序号 宽×高px 比例 是否含透明 PNG 体积`
   - 点标题栏可折叠，不挡地图
   - 若回调里 `image` 为 nil，会明确提示（10.0.2 起自定义 View 里把 `showCrossImage` 设成 YES 就会出现这种情况）
3. **图落地**：`Caches/cross-captures/cross-XXX.png`，只保留最近 12 张。个人测试可直接从 App 沙盒导出看质量。
4. **灯态接口探测**（一次性）
   - 开导航时打印并写入 `Caches/signal-probe.txt`：
     - `AMapNaviTrafficLight` / `AMapNaviTrafficLightInfo` / `AMapNaviSignalLight` / `AMapNaviCross` 这些类存不存在、有哪些属性
     - `AMapNaviDriveDataRepresentable` 协议里有没有 `driveManager:updateTrafficLights:` 之类的方法
   - 同时挂了一个 `driveManager:updateTrafficLights:` 的实现：SDK 真有这个 selector 才会被调到，调到了就把灯态对象逐字段打进调试卡；没有则是死代码，零副作用
   - 结论在「设置 → 调试信息」里显示为「灯态API：…」

## 怎么验

1. 装 IPA → 连圆屏 → 选一条市区路线 → 开始导航（模拟导航也行，会经过路口）
2. 到路口看导航页是否出现「路口图预览」卡，记下尺寸/比例/体积
3. 打开「调试信息」，看「灯态API」那一行：如果出现 `AMapNaviTrafficLight[...remainingTime...]` 之类的属性，说明倒计时可以直接读；如果全是 `×` 和 `opt无`，就只剩截图识别或降级提示两条路

## 安装

unsigned ipa，照常 iLoader 侧载。本版不需要新固件（圆屏仍是 musicA.24）。
