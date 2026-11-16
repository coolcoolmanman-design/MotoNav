# MotoNav firmware v7.12.9-ui.14

## QMI8658 数据读取修复（第 2 轮）
- ui.13 日志：探测/寄存器读写正常（WHO_AM_I=0x05），但 0x35 起 12 字节 burst 读失败（自检 rawG=0, dps=-1）。
- 修复：burst 失败自动降级为单字节逐读（12 次 imuR8）兜底。
- 新增诊断：写入 CTRL1/2/3/7 后回读打印（IMU diag 行）——若回读值不符说明 0x6B 不是 QMI8658（可能是 SH3001 等），据此再定向适配。
