/* RgbBridge — 第二颗 ESP32-S3,把主机的 SPI 帧转成 ST7701S 480×480 RGB565 并口。
 * 引脚按 DevKitC-1(N16R8)分配,避开 flash(26-32)/PSRAM(33-37)/USB(19/20)/BOOT(0)。
 * strap 脚(45/46)仅作输出(init CS / RST),安全。按你的转接板实际接线改这里即可。 */
#pragma once

/* ---------- 面板侧:ST7701S RGB565(16 数据 + 4 同步 + 3线 SPI 初始化)---------- */
#define PANEL_H_RES   480
#define PANEL_V_RES   480

/* ST7701S 初始化序列选择:
 *   1 = 用面板厂商专属序列(LCD_INIT_CMD.txt,Gamma/VCOM/电压针对本屏);
 *   0 = 用 esp_lcd_st7701 组件内置通用默认序列。
 * 上板若偏色/不亮,两版都试一下。 */
#define USE_VENDOR_INIT  1

/* 16 根并口数据(RGB565,esp_lcd 约定顺序:[0..4]=B0..B4,[5..10]=G0..G5,[11..15]=R0..R4) */
#define RGB_B0  4
#define RGB_B1  5
#define RGB_B2  6
#define RGB_B3  7
#define RGB_B4  8
#define RGB_G0  9
#define RGB_G1  10
#define RGB_G2  11
#define RGB_G3  12
#define RGB_G4  13
#define RGB_G5  14
#define RGB_R0  15
#define RGB_R1  16
#define RGB_R2  17
#define RGB_R3  18
#define RGB_R4  21

#define RGB_PCLK   39
#define RGB_DE     40
#define RGB_VSYNC  41
#define RGB_HSYNC  42

/* ST7701S 上电初始化走 3 线 SPI(9-bit,CS/SCL/SDA);初始化完后这几根空闲 */
#define LCD_SPI_CS   45
#define LCD_SPI_SCL  47
#define LCD_SPI_SDA  48

#define LCD_RST      46    /* 复位;-1 = 软复位/外部 RC */
#define LCD_BL       44    /* 背光使能/PWM;-1 = 常亮接 VCC */

/* RGB 时序(来自面板手册 480RGB×480 Timing Table,取 Typ;Thw 取 8 因手册 Typ20/Max8 矛盾)*/
#define RGB_PCLK_HZ            (20 * 1000 * 1000) /* Fclk Typ 20MHz(范围 17~27) */
#define RGB_HSYNC_PULSE        8   /* Thw(Max 8;手册 Typ 20 与 Max 矛盾,取 8) */
#define RGB_HSYNC_BACK_PORCH   60  /* Thbp Typ */
#define RGB_HSYNC_FRONT_PORCH  60  /* Thfp Typ */
#define RGB_VSYNC_PULSE        4   /* Tvw Typ */
#define RGB_VSYNC_BACK_PORCH   20  /* Tvbp Typ */
#define RGB_VSYNC_FRONT_PORCH  20  /* Tvfp Typ */

/* ---------- 主机侧:SPI 从机(接收主机 ESP32 推来的帧)---------- */
#define HOST_SPI_CS    1
#define HOST_SPI_CLK   2
#define HOST_SPI_MOSI  38
#define HOST_SPI_MISO  43   /* 可选:回状态;不接也行 */
