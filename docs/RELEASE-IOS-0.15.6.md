# MotoNav iOS 0.15.6

## 性能
- CoreMotion（MotionG）采样回调从主线程迁到后台队列：0.15.5 起连接圆屏后 App 会持续采样手机传感器，10Hz 回调挂在主队列上是滑动掉帧的可疑来源；现在只有真正发送的一刻（2Hz）回主线程碰 BLE 队列。

## 说明
- 板载 IMU 固件（v7.12.9-ui.9）落地后，G-FORCE 页数据优先来自圆屏本体传感器（SENSOR）；手机 #G 通道保留为无 IMU 板子的兜底（PHONE）。
- 继承 0.15.5：`TIME?` 首连对时应答、CoreMotion #G 推送。
