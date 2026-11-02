# MotoNav firmware v7.12.9-ui.16

## ui.15 实测结论 + 本轮 IMU 深度诊断
- ui.15 已验证的部分：
  - **PWR 菜单配置生效**：日志 `PWR timing: 0x27 0C -> 0C (irq 1s, hw off 10s)`，
    长按中断 1 秒、PMU 硬件关机推到 10 秒。长按菜单 / 关机 / 休眠功能照常可用。
  - 固件正常启动 `MotoNav ready, firmware v7.12.9-ui.15`。
- **IMU 仍未通**：CTRL7 确实写错（0x06→0x08 已修），回读 `CTRL7=03` 正确，
  但 0x35 起的数据寄存器**读不到（NAK）**，自检 rawG=0.000，G 页仍是 NONE。
  说明 0x35 读失败不是 CTRL7 一个原因。

## ui.16 新增诊断（只为定位，不影响功能）
1. **寄存器映射扫描**：在传感器未使能的原生状态下，0x00..0x5F 逐个单字节读，
   打印 `IMU map xx:` 行，`o`=可读 / `X`=NAK——直接看 NAK 边界在哪。
2. **SensorLib 同款初始化补步骤**：加 `CTRL8=0x80`（SensorLib initImpl 的固定步骤）。
3. **状态寄存器体检**：打印 STATUS_INT(0x2D) / STATUS0(0x2E，数据就绪位) /
   TIMESTAMP / TEMPERATURE 原始值。
4. **读法对比**：acc 6 字节 burst、gyro 6 字节 burst、acc 12 字节 burst、
   0x33..0x40 逐字节、以及降到 **100kHz** 后再读一轮（排除高速率下数据段不稳）。

## 判读方法
- 若 map 显示 0x35 起全是 X → 芯片寄存器窗口/接口问题（可能不是 QMI8658，或 I2C 桥接受限）。
- 若 map 全 o 但 STATUS0 恒 0 → 传感器没真正开起来（再看 CTRL7/CTRL9 命令）。
- 若 100kHz 下能读、400kHz 不行 → 总线速率问题，固件里固定降速。
- 若逐字节读到一半 X → burst/跨页问题，改按 6 字节两段读。
