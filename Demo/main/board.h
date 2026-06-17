/* Agent Indicator — Demo 工程 pinmap(ESP32-S3-DevKitC-1 N16R8 + 转接板)
 *
 * 测试用配置:LCD 换 GC9A01(SPI, 240×240 圆屏),音频默认 WM8960,
 * 所有总线同时启用。各组引脚互不冲突(SPI 屏省线后 GPIO 充裕)。
 *   GPIO26-32 = quad flash;GPIO33-37 = octal PSRAM,均不可用。 */
#pragma once

/* ---- LCD GC9A01(SPI, 240×240 圆屏)---- */
#define BOARD_LCD_SPI_CS    10
#define BOARD_LCD_SPI_MOSI  11
#define BOARD_LCD_SPI_SCLK  12
#define BOARD_LCD_SPI_MISO  13   /* GC9A01 只写,可不接;配上无害 */
#define BOARD_LCD_SPI_DC    14
#define BOARD_LCD_RST       -1   /* 软复位(未占用 GPIO) */
#define BOARD_LCD_BL        -1   /* 背光常亮(接 VCC,未占用 GPIO) */
#define BOARD_LCD_PCLK_HZ   (40 * 1000 * 1000)
#define BOARD_LCD_H_RES     240
#define BOARD_LCD_V_RES     240

/* ---- I2C 总线(SDA39/SCL38;触摸/codec/IMU/扩展器/传感器)---- */
#define BOARD_I2C_SDA       39
#define BOARD_I2C_SCL       38
#define BOARD_I2C_FREQ_HZ   400000
#define I2C_ADDR_CST836U    0x15
#define I2C_ADDR_ES8311     0x18
#define I2C_ADDR_WM8960     0x1A
#define I2C_ADDR_TCA9554    0x20
#define I2C_ADDR_INA226     0x40
#define I2C_ADDR_MP2760     0x5C
#define I2C_ADDR_QMI8658C   0x6B

/* ---- TCA9554 扩展位(Demo 转接板无扩展器时各 io_expander_set 自动空操作)---- */
#define EXP_LCD_RST         0
#define EXP_TP_RST          1
#define EXP_LCD_BL_EN       2
#define EXP_PA_EN           3
#define EXP_LED_PWR_EN      4
#define EXP_LCD_SPI_CS      5
#define EXP_CHG_INT         6
#define EXP_TP_INT          7

/* ---- WS2812(两路:matrix=48 / aux=45)---- */
#define BOARD_LED_MATRIX_GPIO   48
#define BOARD_LED_AUX_GPIO      45
#define BOARD_MATRIX_W          8
#define BOARD_MATRIX_H          8
#define BOARD_RING_LEDS         24
#define BOARD_USAGE_LEDS        20
#define BOARD_VU_LEDS           64

/* ---- I2S(默认 WM8960,需要 MCLK)---- */
#define BOARD_I2S_MCLK      1
#define BOARD_I2S_BCLK      2
#define BOARD_I2S_WS        42   /* LRCK */
#define BOARD_I2S_DOUT      40   /* ESP 发送 → codec SDIN(播放) */
#define BOARD_I2S_DIN       41   /* codec 输出 → ESP 接收(录音) */

/* ---- SDMMC 1 线 ---- */
#define BOARD_SD_CLK        4
#define BOARD_SD_CMD        5
#define BOARD_SD_D0         6

/* ---- TWAI(CAN)---- */
#define BOARD_TWAI_TX       47
#define BOARD_TWAI_RX       21

/* ---- 切页按钮(GC9A01 无触摸时用 DevKitC-1 BOOT 键轮播页面)---- */
#define BOARD_BTN           0
