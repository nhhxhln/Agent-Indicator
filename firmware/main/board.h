/* Agent Indicator 主板 pinmap v0.2 — 与 docs/04-schematic-partition.md §2 对应。
 *
 * 目标平台 ESP32-S3-DevKitC-1(N16R8)+ 转接板。引脚约束:
 *   - GPIO26-32 = quad flash;**GPIO33-37 = octal PSRAM**,全部不可用!
 *     (v0.1 曾误把 I2S 放在 33/34,已修正为 42/44)
 *   - GPIO19/20 = 原生 USB:做 USB-Serial-JTAG(日志+下载+REPL)
 *   - 可用普通 GPIO 仅 0-18,21,38-48 共 29 个,跑满 480×480 RGB 屏(20 根)后
 *     余量不足以同时再上 SD + CAN,故默认 Profile A(Display+Audio+背光),
 *     SD/CAN 由 Kconfig 开关(与 I2S/背光引脚复用),详见 docs/04 §DevKitC-1。 */
#pragma once

/* ---- RGB LCD (ST7701, 16-bit RGB565) ---- */
#define BOARD_LCD_PCLK      14
#define BOARD_LCD_VSYNC     45
#define BOARD_LCD_HSYNC     46
#define BOARD_LCD_DE        47
#define BOARD_LCD_D_B0      4   /* B0..B4: 4,5,6,7,15  */
#define BOARD_LCD_D_B1      5
#define BOARD_LCD_D_B2      6
#define BOARD_LCD_D_B3      7
#define BOARD_LCD_D_B4      15
#define BOARD_LCD_D_G0      16  /* G0..G5: 16,17,18,8,3,48 */
#define BOARD_LCD_D_G1      17
#define BOARD_LCD_D_G2      18
#define BOARD_LCD_D_G3      8
#define BOARD_LCD_D_G4      3
#define BOARD_LCD_D_G5      48
#define BOARD_LCD_D_R0      9   /* R0..R4: 9,10,11,12,13 */
#define BOARD_LCD_D_R1      10
#define BOARD_LCD_D_R2      11
#define BOARD_LCD_D_R3      12
#define BOARD_LCD_D_R4      13
#define BOARD_LCD_H_RES     480
#define BOARD_LCD_V_RES     480

/* ---- I2C 总线(触摸/IMU/扩展器/电源) ---- */
#define BOARD_I2C_SDA       38
#define BOARD_I2C_SCL       39
#define BOARD_I2C_FREQ_HZ   400000
#define I2C_ADDR_CST820     0x15
#define I2C_ADDR_TCA9554    0x20
#define I2C_ADDR_INA226     0x40
#define I2C_ADDR_MP2760     0x5C
#define I2C_ADDR_QMI8658C   0x6B

/* ---- TCA9554 扩展位 ---- */
#define EXP_LCD_RST         0
#define EXP_TP_RST          1
#define EXP_LCD_BL_EN       2
#define EXP_PA_EN           3
#define EXP_LED_PWR_EN      4
#define EXP_LCD_SPI_CS      5
#define EXP_CHG_INT         6
#define EXP_TP_INT          7

/* ---- WS2812B ---- */
#define BOARD_LED_MATRIX_GPIO   40  /* RMT CH: matrix 链,1~4 块 8x8 */
#define BOARD_LED_AUX_GPIO      41  /* circle(24) + usage bar(20) + 拾音条(64) */
#define BOARD_MATRIX_W          8
#define BOARD_MATRIX_H          8
#define BOARD_RING_LEDS         24
#define BOARD_USAGE_LEDS        20
#define BOARD_VU_LEDS           64

/* ---- I2S (ES8311, MCLK-from-SCLK) ---- */
#define BOARD_I2S_BCLK      1
#define BOARD_I2S_WS        21
#define BOARD_I2S_DOUT      42   /* v0.2: 原 33(octal PSRAM)→ 42 */
#define BOARD_I2S_DIN       44   /* v0.2: 原 34(octal PSRAM)→ 44 */

/* ---- LCD 背光 PWM(恒流驱动 IC 的 DIM/PWM 脚,调光)---- */
#define BOARD_LCD_BL_PWM    43   /* LEDC 调光;EN(开关)走 TCA9554 EXP_LCD_BL_EN */

/* ---- SDMMC 1-bit ⚠ 默认禁用(CONFIG_AGENTIND_ENABLE_SD) ----
 * D0=42 与 I2S_DOUT 复用;启用 SD 需禁用音频或在转接板改接。 */
#define BOARD_SD_CLK        2
#define BOARD_SD_CMD        0
#define BOARD_SD_D0         42

/* ---- TWAI(CAN)⚠ 默认禁用(CONFIG_AGENTIND_ENABLE_CAN)----
 * TX=43 与背光 PWM、RX=44 与 I2S_DIN 复用;TWAI 经 GPIO matrix 也可改接其他空脚。 */
#define BOARD_TWAI_TX       43
#define BOARD_TWAI_RX       44
