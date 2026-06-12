/* UI 多语言:中/英字符串表,NVS 持久化。
 * 注意:中文渲染需要 CJK 字体(/spiffs/fonts/ui.ttf 放含中文的 TTF,
 * 内置 Montserrat 无 CJK 字形,缺字会显示占位框)。 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_LANG_EN = 0,
    UI_LANG_ZH = 1,
} ui_lang_t;

typedef enum {
    STR_WELCOME = 0,      /* 开机欢迎/占位 */
    STR_WAIT_HOST,        /* 等待上位机 */
    STR_STATE_IDLE,
    STR_STATE_CONNECTING,
    STR_STATE_THINKING,
    STR_STATE_RESPONDING,
    STR_STATE_TOOL_USE,
    STR_STATE_WAITING,
    STR_STATE_ERROR,
    STR_STATE_RATELIMIT,
    STR_STATE_OFFLINE,
    STR_INPUT,
    STR_OUTPUT,
    STR_BATTERY,
    STR_MAX,
} ui_str_id_t;

void ui_i18n_init(void);            /* 读 NVS,默认 EN */
const char *tr(ui_str_id_t id);
ui_lang_t ui_lang_get(void);
esp_err_t ui_lang_set(ui_lang_t lang); /* 写 NVS 并立即生效 */
const char *tr_state(int agent_state); /* agent_state_t → 状态名 */

#ifdef __cplusplus
}
#endif
