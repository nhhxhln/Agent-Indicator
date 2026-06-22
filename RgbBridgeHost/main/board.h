/* 主机侧(主板 ESP32-S3)→ RgbBridge 的 4 线 SPI。
 * 这 4 个脚在主板上任选空闲 GPIO,接到桥片的 HOST_SPI_*(CS/CLK/MOSI/MISO)。
 * 示例用主板空闲脚 15/16/17/18(见 Demo 空闲引脚分析)。 */
#pragma once

#define PIN_CS    15
#define PIN_CLK   16
#define PIN_MOSI  17
#define PIN_MISO  18    /* 可选(回状态);桥片当前不回数据,接不接都行 */

#define LINK_CLK_HZ  (20 * 1000 * 1000)  /* 20MHz,稳;够 ~5fps@565。可上调试更高 */

#define PANEL_W  480
#define PANEL_H  480
