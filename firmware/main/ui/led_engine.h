/* LED 渲染引擎:60fps 任务,按 g_app 状态驱动 matrix/ring/bars。
 * RMT CH0 = matrix 链(1/4 块),CH1 = ring(24)+usage(20)+VU(64) 一条链。 */
#pragma once

#include "esp_err.h"

esp_err_t led_engine_start(void);
