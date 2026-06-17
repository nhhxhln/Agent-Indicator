/* LED 渲染引擎:60fps 任务,按 g_app 状态驱动 matrix/ring/bars。
 * RMT CH0 = matrix 链(1/4 块),CH1 = ring(24)+usage(20)+VU(64) 一条链。 */
#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 灯效模式(参考 OpenRGB 预设)。AGENT = 默认的状态驱动渲染,
 * 其余为全灯带覆盖灯效(matrix+circle+bars 统一作一条逻辑灯带) */
typedef enum {
    LED_FX_AGENT = 0,
    LED_FX_SOLID,      /* 常亮,颜色可设 */
    LED_FX_BREATH,     /* 呼吸 */
    LED_FX_MARQUEE,    /* 跑马灯 */
    LED_FX_RAINBOW,    /* 彩虹流动 */
    LED_FX_OFF,
    LED_FX_MAX,
} led_fx_t;

/* 灯带目标:两路 WS2812 可分别控制 */
typedef enum {
    LED_TGT_ALL = 0,   /* matrix + aux 都设 */
    LED_TGT_MATRIX,    /* 仅 8×8 矩阵(GPIO48) */
    LED_TGT_AUX,       /* 仅 ring+usage+vu 链(GPIO45) */
} led_target_t;

esp_err_t led_engine_start(void);
/* speed 1-100;rgb 仅 SOLID/BREATH/MARQUEE 使用 */
void led_engine_set_fx(led_fx_t fx, uint8_t r, uint8_t g, uint8_t b, uint8_t speed); /* = 全部 */
void led_engine_set_fx_target(led_target_t target, led_fx_t fx,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t speed);
led_fx_t led_engine_get_fx(void);                  /* matrix 的当前模式 */
led_fx_t led_engine_get_fx_target(led_target_t target);

/* 通知爆闪:两路全亮指定颜色闪烁 times 次,结束自动恢复原灯效(非阻塞) */
void led_engine_flash(uint8_t r, uint8_t g, uint8_t b, int times);

#ifdef __cplusplus
}
#endif
