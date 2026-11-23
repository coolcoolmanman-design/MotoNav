# MotoNav · sdgoods（谷仓次元屏）板级

Arduino 路线 A′：从 `firmware/boards/185b` 分叉，适配谷仓引脚，并尽量作为 **MULTI 可切换固件槽** 交付（保留官方 launcher）。  
标识：`v7.12.3-sdgoods` · `esp_app_desc.project_name` / app_id = **`MotoNav`**。

**不要**刷 1.75 或未改引脚的 185B 镜像到谷仓板。

## 硬件对照（已写入 `pin_config.h`）

| 信号 | 本板 GPIO | 相对 185B |
|---|---|---|
| QSPI D0..D3 / SCK / CS / TE | 46/45/42/41 / 40 / 21 / 18 | 相同 |
| LCD RST | **9** | 185B=3 |
| LCD BL | **13**（LEDC） | 185B=5 |
| LCD POW_EN | **12**（BSP 上电拉低） | 185B 无 |
| I2C SDA/SCL | 11/10 | 相同 |
| TP INT / RST | 4 / **5** | 185B RST=1 |
| 电池自锁 | **7**（高保持） | 不同模型 |
| 电池 ADC | **8**（ADC1_CH7，×3） | 185B=BQ27220 |
| 电源键 | **6**（低有效） | 185B 无同款抽象 |
| 音频 PA | **3** | 引脚组不同 |

## 构建

```bash
./firmware/build-sdgoods.sh
# 或 Windows: .\firmware\build.ps1 -Board sdgoods -PanelVariant 0
```

FQBN 暂与 185B 相同（16MB / `app3M_fat9M_16MB` / OPI / CDC）。运行时以**设备上的官方分区表**为准。

产物（`firmware/dist/sdgoods/panel0/`）：

| 文件 | 用途 |
|---|---|
| **`MotoNav_app.bin`** | **MULTI 主交付物**：纯 app 镜像（`pack_app` 校验） |
| `MotoNav_app.meta.json` | app_id / sha256 / 交付说明 |
| `*_app_0x10000.bin` | 同上内容的别名 |
| `*_full_0x0.bin` | **仅 SINGLE / 救砖**：含 bootloader+分区，写 0x0 会覆盖 launcher |

构建脚本会调用 `sdgoods-esp32s3/tools/pack_app.py --no-emit`；`project_name` 必须为 `MotoNav`（见 `EspAppDesc.cpp`）。

## MULTI vs SINGLE 安装

### MULTI（推荐：不擦 launcher）

前置：设备已有官方启动器 + 分区表（不要先刷 `*_full_0x0.bin`）。

1. 构建 → 取 **`MotoNav_app.bin`**。
2. 经启动器安装通道 / 开放平台装进空 `ota_N`（或调试：`esptool write_flash <slot_off> MotoNav_app.bin`，再靠 launcher `self_check` 收编）。
3. 从 launcher 启动该槽。
4. **返回启动器**：电源键长按约 1.5 s，或空闲关机计时触发时走 `esp_ota_set_boot_partition(factory)` + `esp_restart`（等价 `sdgoods_return_to_launcher` 契约；仅当 factory 为 `SDGOODS_LAUNCHER` 且本固件跑在 ota 槽时生效）。
5. 串口应见：`app_id=MotoNav … managed=1`，返回时 `return-to-launcher: boot=factory, restarting`。

### SINGLE（整机导航 / 救砖）

1. 构建 → 取 `*_full_0x0.bin`。
2. `esptool write_flash 0x0 …_full_0x0.bin`（**覆盖 launcher**）。
3. 此时无启动器可回；长按电源键不会 return；空闲路径释放自锁并深睡。

## 相对硬件仍未知 / 未验证

1. ST77916 初始化仍复用 185B 表；MADCTL / TE 真机未验证。
2. 触摸 `BOARD_TOUCH_FLIP_*` 默认 0。
3. 充电 / USB 状态暂恒 0；电量曲线未标定。
4. Arduino 编译期 FlashSize=16MB vs 设备 32MB 官方表：MULTI 下需真机回归。
5. 未接平台控制中心 / appdata SDK（上架级仍建议 IDF guest）。
6. 本 VM 无串口：未做真机装槽。

## 日后「改过的 1.75」如何落在本板路径

1. **不要**覆盖本板 `pin_config` / `EspAppDesc` / `SdgoodsMulti` 契约。
2. 业务与协议从改装 1.75 合入；显示仍走 ST77916 + `PortScale`。
3. 保持 app_id=`MotoNav` 与 return-to-launcher，便于继续进 MULTI 槽。

调研：`docs/motonav-port-to-sdgoods.md`、`docs/motonav-sdgoods-multi-fw.md`。
