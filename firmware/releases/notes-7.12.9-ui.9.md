# MotoNav firmware v7.12.9-ui.9

## G-FORCE 页改用板载 IMU 真实数据
- 新增 `src/imu/Imu.h`：开机运行时探测板载 IMU（QMI8658 / LSM6DSL / SC7A20），带静止约 1g 自检。
- G 页数据优先级：**板载 IMU（标 SENSOR）> 手机 #G（标 PHONE）> 模拟（标 SIM）**；ROT 转速同步走 IMU 陀螺。
- 速度数字不再补前导零（`86` 而不是 `086`）。

## TPMS 胎压页
- 仍为模拟数据（蓝牙气嘴硬件未接入，协议未定）。

## 其他
- 继承 ui.8：副页任意方向滑动回主页、`TIME?` 首连对时兜底、断开后正确显示 no link（TX 订阅判据）。
