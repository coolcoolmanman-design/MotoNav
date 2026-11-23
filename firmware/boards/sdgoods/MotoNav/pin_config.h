#pragma once

// 谷仓次元屏（sdgoods-esp32s3）板级引脚。
// QSPI / I2C / TP_INT 与 1.85B 同族；RST / 背光 / POW_EN / TP_RST / 电源与 185B 不同。
// 引脚来源：sdgoods-esp32s3/components/sdgoods_board/include/board_pins.h
#define LCD_SDIO0 46
#define LCD_SDIO1 45
#define LCD_SDIO2 42
#define LCD_SDIO3 41
#define LCD_SCLK 40
#define LCD_CS 21
#define LCD_RESET 9
#define LCD_BL 13
#define LCD_TE 18
#define LCD_POW_EN 12
// sdgoods BSP lcd_pow_on() drives this pin LOW at panel bring-up (treat as active-low enable).
#define LCD_POW_EN_ON_LEVEL 0
#define PANEL_WIDTH 360
#define PANEL_HEIGHT 360
// 逻辑画布保留完整原布局，统一缩放到物理面板，不裁剪 HUD 或地图。
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#define LCD_INIT_VARIANT 0   // 0=读面板 ID 自动选择；1/2=指定官方初始化序列（暂复用 185B 序列）

#define IIC_SDA 11
#define IIC_SCL 10
#define TP_INT 4
#define TP_RESET 5
// 触摸翻转：真机未验证；先按不翻转。方向不对时改 1。
#define BOARD_TOUCH_FLIP_X 0
#define BOARD_TOUCH_FLIP_Y 0

// 电池自锁 MOSFET：高电平保持上电（BOARD_BAT_CONTROL_LATCH_LEVEL=1）
#define BAT_LATCH 7
#define BAT_LATCH_ON_LEVEL 1
// 电池电压 ADC：GPIO8 / ADC1_CH7，分压后 ×3 得电芯电压（与 sdgoods_hw_bat_v 一致）
#define BAT_ADC_PIN 8
#define BAT_ADC_SCALE 3.0f
// 电源键：低有效
#define PWR_KEY 6
#define PWR_KEY_ACTIVE_LEVEL 0

#define I2S_MCK_IO 15
#define I2S_BCK_IO 48
#define I2S_DI_IO 17
#define I2S_WS_IO 38
#define I2S_DO_IO 47
#define PA 3
const int SDMMC_CLK=-1;
const int SDMMC_CMD=-1;
const int SDMMC_DATA=-1;
const int SDMMC_CS=-1;
