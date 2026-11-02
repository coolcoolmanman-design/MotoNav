# MotoNav 1.75 固件 v7.12.10-ui.2

> 合入上游 `zyn950929/motonav` 10-26 提交（cc65154 / 7.12.10-pages.4）的固件级修复。
> 基于 ui.1（G 页布局调整），UI 无变化。

## 改动

### MotoNavIpcStackGuard 栈守卫（修开机重启，上游同款）

- 现象：蓝牙初始化阶段 ESP-IDF 的两个 IPC 任务（ipc0/ipc1）默认栈只有 1024B，
  BLE 协议栈初始化路径可能栈越界 → 开机即重启（上游抓到真实崩溃转储并校验）。
- 修复：链接期 `--wrap=xTaskCreatePinnedToCore` 钩子，仅当任务名为 ipc0/ipc1、
  优先级为最高-1、core 实参与任务名一致且申请栈 <4096B 时提升到 **4096B**，
  其他任务一律原样透传。总代价约 6KB 动态内存。
- 构建同步：`firmware/build.py` 增加
  `--build-property compiler.c.elf.extra_flags=-Wl,--wrap=xTaskCreatePinnedToCore`
  （arduino-cli 不读 library.properties 的 ldflags，必须显式传）。
  若漏传该参数，`__real_xTaskCreatePinnedToCore` 未定义会在链接期直接报错，
  不会静默产出仍带 1KB 栈的固件。
- 开机自检：蓝牙初始化完成后打印
  `IPC stacks: ipc0=4096B freeMin=… ipc1=4096B freeMin=…`（bytes=4096 即守卫生效）。

上游已做 8 次"关机静置 2 分钟 + 手机蓝牙断开 + 纯电池开机"受控重启验证，
全部一次开机成功；刷前刷后 NVS 逐字节一致。

## 继承 ui.1

G-FORCE 页布局：RIDE 上表盘、BATT 左栏与 ROT 逐行镜像、PEAK G/MAX 框架下移 16px。

## 刷机

- 仅 app 分区（不擦 NVS）：`write-flash 0x10000 MotoNav_universal_1.75_v7.12.10-ui.2_panel0_app_0x10000.bin`
- 完整镜像：`MotoNav_universal_1.75_v7.12.10-ui.2_panel0_full_0x0.bin`
