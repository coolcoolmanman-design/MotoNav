# MotoNav iOS 0.15.5 · 传感器 G 值通道 + 时间同步兜底

基于 iOS 0.15.4 源码迭代。两项配套固件 v7.12.9-ui.7+ 的协议增强：把手机运动传感器的真实 G 值推给圆屏 G-FORCE 页，并为首次蓝牙配对后待机时钟不同步增加双向兜底。本轮由 AI 开发；随发布流水线自动打包未签名 IPA。

## 变更内容

| 项目 | 0.15.5 的实现 |
|---|---|
| `#G` 传感器通道 | 新增 `MotionG`（CoreMotion）：连接后以 10Hz 采样设备运动，节流到每 500ms 发一条 `#G,<总G>,<转速°/s>`；总 G = 重力 + 用户加速度合矢量（单位 G，静止 ≈ 1.00），转速 = 旋转角速度模长（°/s）。用 `sendOnce` 发送（不进重连补发缓存，避免旧值污染） |
| 生命周期 | `onLinkReady` 启动推送、断开连接 / 蓝牙关闭时停止，不额外耗电；通道就绪判定 `sensorChannelReady = isReady && txNotifyReady` |
| 时间同步兜底 | 收到圆屏发来的 `TIME?` 通知（固件未对时时每 10 秒询问一次）→ 立即 `syncScreenClock(force: true)` 回发 `#TIME`；与既有连接对时 / 每小时校准互补 |
| 版本号 | `RELEASE_VERSION`、`APP_VERSION`、`MARKETING_VERSION` 同步 0.15.5 |

## 配套固件

- 固件 v7.12.9-ui.7+：G 值页接 `#G`（3 秒内收到过标 PHONE，超时退回模拟并标 SIM），右栏新增 ROT D/S；未对时主动发 `TIME?`。
- 旧固件 / 旧 App 交叉兼容：旧 App 收到 `TIME?` 无害忽略；旧固件忽略 `#G`。

## 验证边界

- CI 内完成：既有 Swift 回归测试 + iPhone arm64 Release 编译 + 未签名 IPA 打包。
- 未做：真机 CoreMotion 数值标定（手机固定位置静止时应显示约 1.00G，剧烈晃动峰值上升）；G 值在骑行姿态下的安装方向差异未做轴分解，仅合成矢量模长。
- 源码不含 Apple 证书、描述文件、私人签名或地图 / 天气 Key；Key 均为运行时在 App 内填写。
