# MotoNav firmware v7.12.9-ui.17 — 板载 IMU 最终修复

## ui.16 诊断结论（实机日志）
寄存器映射扫描 0x00..0x5F **全部可读**（无 NAK）；CTRL7=0x03 回读正确；
`STATUS0=0x03`（加速度+陀螺数据就绪）；温度 0x1FB5 = 31.7°C（合理）；
加速度 Z 轴 = 0x107E / 4096 = **1.03g（正是重力）**，陀螺输出仅 ±几十 LSB。
**结论：芯片、总线、寄存器配置、数据输出全部正常。**

## 真因（第 2 个，也是最后一个）
自检代码调的是 `imuPoll()`，而 `imuPoll()` 开头有 `if (!imuOK) return false;`；
`imuOK` 又要等自检通过才置位 → **自检永远返回 false，任何芯片都探测不出来**。
（ui.9 引入该门卫后，板载 IMU 从未真正通过自检；ui.13/ui.14/ui.15 的 CTRL 修法都对，
只是全被这个死循环挡住了。）

## ui.17 修复
- 拆两层：`imuSample()`（原始采样，无门卫，自检用）/ `imuPoll()`（imuOK 门卫，G 页运行时用）。
- 移除 ui.16 的批量诊断打印（寄存器映射、逐字节 dump、降速测试），保留一行
  `IMU diag: ... STATUS0=.. temp=..C` 用于确认。

## 预期日志
```
IMU probe 0x6B: id00=0x05 ...
IMU diag 0x6B: CTRL1=40 CTRL2=25 CTRL3=55 CTRL7=03 STATUS0=03 temp=31.7C
IMU: QMI8658 (type 1 addr 0x6B)
PWR timing: 0x27 0C -> 0C (irq 1s, hw off 10s)
MotoNav ready, firmware v7.12.9-ui.17
```
G 页数据源标注应为 **SENSOR**，静止显示 0.00，晃动出动态值。

## 附：电源菜单（ui.15 引入，已实测配置生效）
长按 PWR 约 1 秒 → 菜单（SHUTDOWN / SLEEP）；点屏幕选择；
PMU 硬件关机推到 10 秒（`PWR timing: 0x27 0C -> 0C` 已确认）。
