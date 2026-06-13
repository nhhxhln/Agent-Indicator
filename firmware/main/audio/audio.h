/* 音频:ES8311(I2S)+ NS4150B。提示音播放 + MIC RMS → g_app.mic_level_db。 */
#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_start(void);
void audio_play_tone(uint8_t tone_id); /* 0 done / 1 attention / 2 error / 3 boot */
const char *audio_codec_name(void);    /* "ES8311" / "WM8960" / "none" */
void audio_set_volume(int vol);        /* 0-100,分发到在位 codec */
bool audio_ready_c(void);

#ifdef __cplusplus
}
#endif
