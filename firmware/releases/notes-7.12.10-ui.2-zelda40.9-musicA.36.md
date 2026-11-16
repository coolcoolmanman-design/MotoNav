# MotoNav 固件 7.12.10-ui.2-zelda40.9-musicA.36

配套 iOS：**0.16.2.32**（本版不需要升级 App）

## 断连时的车速兜底清零

排查「六槽速度是不是实时」时顺带确认了车速的完整生命周期。先澄清一个容易看漏的点：

**正常断连其实已经会清零。** 主循环 `MotoNav.ino:257` 检测 `conn` 由 true 变 false 时调
`resetNavState()`，其中 `CommandReceiver.h:16` 有 `speedKmh = -1`，接着 `drawWaiting()`
整页重画，六槽 / G 页 / 导航 HUD 都会显示 0。只盯着 `onDisconnect()` 回调会误以为没清。

**但回调层确实缺一道兜底**，有一段时序会漏掉：

1. App 点断开先发 `#OFF` → `appBye = true` → `conn` 立即变 false → 主循环清零一次；
2. BLE 链路此时可能还没断（系统级 HID 会常驻/自动重连）；
3. 若这期间又收到任意消息，`handleMessage()` 会把 `appBye` 重置为 false 并可能写入 `#v`；
4. 链路随后真正断开时，`conn` 已经/仍是 false，**没有状态变化沿**，主循环不会再 reset，
   `speedKmh` 就停在最后一次读数。

### 本版改动

`src/connectivity/Bluetooth.h` 的 `ServerCB::onDisconnect()` 增加一行 `speedKmh = -1`。
`DeviceContext.h` 在它之前 include，全局可见；单个对齐 int 写入在 BLE 回调线程里是安全的。
清零后待机页靠 `animateCat()` 每 100ms 一拍重画，导航/地图页靠 `navDirty` 重画，无需额外触发。

## 影响范围

- 只动固件一行 + 注释，**App 0.16.2.32 不用重装**，协议、尺寸、配色、布局全部不变。
- 短暂断连又立刻重连时，速度会瞬间显示 0，随后 App 重新推 `#v` 恢复（比残留旧值更合理）。
- 六槽速度槽 (`fn=0`)、G 页大速度、导航 HUD 超速判红共用同一个 `speedKmh`，一起生效。
- `#p,...` 的 `posKmh` 是位置推算用，不受影响。

## 兼容性

- 路口放大图：272×150 全图 / 136×75 预览、ver=2 5 色调色板、bulk type 9/10 均未动；
  musicA.35 的按代静默窗、musicA.34 的车道胶囊 `CROSS_LANE_Y=224` 保持不变。
- 新固件 + 旧 App、旧固件 + 新 App 均无协议差异。
