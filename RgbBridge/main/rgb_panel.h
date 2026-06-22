/* ST7701S 480×480 RGB565 面板封装(3线 SPI 初始化 + RGB 并口像素)。 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rgb_panel_init(void);

/* 把一块 RGB565 位图画到 (x,y)~(x+w,y+h)。坐标右下为开区间。 */
esp_err_t rgb_panel_draw(int x, int y, int w, int h, const void *px565);

/* 画整屏 RGB565(buf 长度 = 480*480) */
esp_err_t rgb_panel_blit_full(const uint16_t *fb565);

#ifdef __cplusplus
}
#endif
