/* LCD(ST7701 RGB + CST836U)+ LVGL9。
 * 初始化失败(裸开发板无屏)时系统继续无头运行,display_ready() 返回 false,
 * 文本流降级为日志输出;各 case 据此选择 GUI 或日志可视化。 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t display_start(void);
bool display_ready(void);
void display_set_backlight(int pct); /* LCD 背光亮度 0-100(LEDC PWM) */

#ifdef __cplusplus
}
#endif
