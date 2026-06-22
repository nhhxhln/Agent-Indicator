/* 自测图案:不接主机也能循环点亮面板(纯色/彩条/渐变/棋盘/边框)。 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

extern volatile bool g_host_active; /* link_spi 收到主机帧后置 1,自测暂停 */

esp_err_t patterns_start(void);

#ifdef __cplusplus
}
#endif
