/* 音频:ES8311(I2S)+ NS4150B。提示音播放 + MIC RMS → g_app.mic_level_db。 */
#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_start(void);
void audio_play_tone(uint8_t tone_id); /* 0 done / 1 attention / 2 error / 3 boot */

#ifdef __cplusplus
}
#endif
