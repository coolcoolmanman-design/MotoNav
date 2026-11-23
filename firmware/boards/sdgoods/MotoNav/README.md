# MotoNav · sdgoods（谷仓次元屏）板级

Arduino 路线 A 板级移植：从 `firmware/boards/185b` 分叉，适配谷仓 `sdgoods-esp32s3` 引脚与电源模型。  
标识：`v7.12.3-sdgoods`。目标：在真机点亮 ST77916 + CST816，并复用现有 BLE/导航逻辑画布（466→360 `PortScale`）。

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

## 构建（MVP 门禁 = 编译成功）

FQBN 暂与 185B 相同（16MB / `app3M_fat9M_16MB` / OPI PSRAM / CDC）。谷仓实物为 **32MB Flash + 8MB Octal PSRAM**；量产分区需另定，见下方未知项。

```bash
# Linux / macOS（需 arduino-cli + esp32:esp32@3.3.10）
./firmware/build-sdgoods.sh

# Windows
.\firmware\build.ps1 -Board sdgoods -PanelVariant 0
```

产物目录：`firmware/dist/sdgoods/panel0/`。

## 相对硬件仍未知 / 未验证

1. **ST77916 初始化与 MADCTL**：当前复用 185B 两套 vendor 表；谷仓 BSP 使用 `esp_lcd_st77916` + `BOARD_ST77916_VENDOR_MADCTL=0x60`。真机若色序/旋转不对，优先改 `LCD_INIT_VARIANT` 或对照 `st77916_vendor_init.inc`。
2. **触摸坐标系**：`BOARD_TOUCH_FLIP_X/Y` 默认 0；安装角度与 MADCTL 对齐后需实机标定。
3. **充电 / USB 在位**：公开 BSP 无独立充电状态脚；`V,pct,chg,usb,mV` 里 chg/usb 暂恒为 0。
4. **电量曲线**：ADC×3 电压→百分比为粗表，需按 500mAh 电芯标定。
5. **Flash 32MB / 分区**：当前 Arduino 分区仍按 16MB；整机刷写会与官方 launcher/appdata 布局冲突（本板级按独立导航固件设计）。
6. **电源键语义**：空闲关机路径会释放自锁并深睡，按键唤醒；短按/长按导航手势未接平台控制中心。
7. **TE 同步**：引脚已定义，Arduino 快速刷屏路径是否用 TE 与 185B 相同（实验级）。

## 日后「改过的 1.75」如何落在本板路径

1. **不要**把 1.75 的 `pin_config.h` / CO5300 / AXP2101 直接覆盖进来。
2. 业务与协议（`CommandReceiver`、地图、页面逻辑）从改装后的 1.75 **合入本目录或抽到共享 `src/`**，显示仍走本板的 ST77916 + `PortScale`。
3. 板级 HAL 边界保持在本目录：`pin_config.h`、`BoardSdgoodsPanel.*`、电池 ADC/自锁、背光/`POW_EN`。
4. 若 1.75 改装已模块化 `DeviceContext` / `DisplayPipeline`，优先让它们依赖「逻辑 466 画布」接口，物理 360 刷新留在本板 `flushLogicalFrame` / `PortScale`。

调研全文：仓库外 Project 文档 `docs/motonav-port-to-sdgoods.md`。

## 有板后的烧录步骤（简述）

1. 编译得到 `*_app_0x10000.bin` / `*_full_0x0.bin`。
2. 用 `esptool` 按 0x0 写 full 镜像（会覆盖官方 launcher；仅适合整机导航固件试验）。
3. USB-CDC 串口看 `sdgoods panel ID …`、`touch ok`、`sdgoods bat ADC …`。
4. 确认纯色/圆环与触摸方向；必要时改 `LCD_INIT_VARIANT` 与 `BOARD_TOUCH_FLIP_*`。
