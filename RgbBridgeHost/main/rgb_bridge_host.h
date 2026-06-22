/* 主机侧 SPI 主驱动:按 RgbBridge 协议把帧/行推给桥片。
 * 协议(模式0,每事务一行包):[0xA9][fmt][y_lo][y_hi][W 像素]
 *   fmt: 0=RGB565 小端(2B/px),1=RGB888(3B/px)。 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RGB_FMT_565  0
#define RGB_FMT_888  1

esp_err_t rgb_bridge_host_init(int clk_hz);

/* 推一行:line 指向该行 W 个像素(按 fmt 的字节数)。 */
esp_err_t rgb_bridge_push_line(int y, int fmt, const void *line);

/* 推整帧:fb 为 W*H 像素的连续缓冲(按 fmt)。 */
esp_err_t rgb_bridge_push_frame(const void *fb, int fmt);

#ifdef __cplusplus
}
#endif
