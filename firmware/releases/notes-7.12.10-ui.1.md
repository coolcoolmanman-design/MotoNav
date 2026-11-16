# MotoNav 1.75 固件 v7.12.10-ui.1

> 版本号从本版起切换为 7.12.10 编号（7.12.9 系列到此为止：ui.3 ~ ui.17）。
> 基于 ui.17（IMU 修复 + PWR 电源菜单），仅改 G-FORCE 页布局。

## 改动（G-FORCE 页布局调整，已按效果图 v3 定稿）

1. **RIDE 计时移到速度表盘正上方居中**（y≈184，大 G 值与表盘顶之间的空档），不再挤在侧栏。
2. **电量 BATT 移到左栏**，并与右栏 ROT D/S 逐行对齐：
   - 左栏：PEAK G / 峰值 + BATT / 电量%
   - 右栏：MAX / 最高车速 + ROT D/S / 旋转角速度
   - 标签行 y=262、数值行 y=276，两栏完全镜像对称。
3. **PEAK G / MAX 框架整体下移 16px**（标签 y 184→200，数值 y 202→218），视觉重心与表盘中线齐平。

## 继承上一版（ui.17）

- 板载 IMU 修复：CTRL7 地址 0x06→0x08 + 自检死循环拆分（imuSample/imuPoll）。
- PWR 长按电源菜单：长按 1s 弹出 SHUTDOWN / SLEEP；SLEEP 深睡点屏唤醒。

## 刷机

- 完整镜像：`MotoNav_universal_1.75_v7.12.10-ui.1_panel0_full_0x0.bin`
- 仅 app 分区（不擦 NVS，保留时钟/设置）：`write-flash 0x10000 MotoNav_universal_1.75_v7.12.10-ui.1_panel0_app_0x10000.bin`
