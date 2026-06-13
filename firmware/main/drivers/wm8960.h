/* WM8960 立体声音频 codec 最小驱动(I2C 0x1A,共享总线)。
 * 与 ES8311 二选一运行时探测;WM8960 **需要 MCLK**(BOARD_I2S_MCLK),
 * 不能像 ES8311 那样用 SCLK 当 MCLK。 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

bool wm8960_probe(void);                 /* I2C 0x1A 在位探测 */
esp_err_t wm8960_init(int sample_rate);  /* 16bit I2S 从机,DAC→HP/SPK,LINPUT1→ADC */
void wm8960_set_volume(int vol);         /* 0-100 → HP/SPK 输出音量 */

#ifdef __cplusplus
}
#endif
