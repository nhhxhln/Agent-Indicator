#include "app_state.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "proto/proto.h"
#include "sdkconfig.h"

static const char *TAG = "state";

app_state_t g_app;
uint32_t g_fx_color_cache = 0x0080ff; /* CONFIG key=5 暂存,key=4 时生效 */
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static RingbufHandle_t s_text_rb;

void app_state_init(void)
{
    memset(&g_app, 0, sizeof(g_app));
    g_app.state = AGENT_OFFLINE;
    memset(g_app.usage_pct, 0xFF, sizeof(g_app.usage_pct));
    g_app.brightness = 160;
    g_app.matrix_tiles = CONFIG_AGENTIND_MATRIX_TILES;
    s_text_rb = xRingbufferCreate(TEXT_RING_SIZE, RINGBUF_TYPE_NOSPLIT);
}

void app_text_push(uint8_t stream, uint8_t op, const char *utf8, uint16_t len)
{
    /* 头 2 字节:stream + op,display 任务按需处理 clear/replace */
    uint8_t hdr[2] = { stream, op };
    uint8_t item[258];
    if (len > 256) len = 256;
    memcpy(item, hdr, 2);
    memcpy(item + 2, utf8, len);
    xRingbufferSend(s_text_rb, item, len + 2, 0); /* 满了就丢,显示流允许丢帧 */
}

int app_text_pop(char *buf, int maxlen, uint8_t *stream)
{
    size_t sz = 0;
    uint8_t *item = xRingbufferReceive(s_text_rb, &sz, 0);
    if (!item) return 0;
    int n = (int)sz - 2;
    if (n > maxlen) n = maxlen;
    if (stream) *stream = item[0];
    memcpy(buf, item + 2, n);
    vRingbufferReturnItem(s_text_rb, item);
    return n;
}

void app_state_apply_frame(uint8_t type, const uint8_t *p, uint16_t len)
{
    taskENTER_CRITICAL(&s_lock);
    g_app.last_rx_us = esp_timer_get_time();
    switch (type) {
    case PROTO_MSG_STATE:
        if (len >= 2 && p[0] < AGENT_STATE_MAX) {
            g_app.state = (agent_state_t)p[0];
            g_app.state_detail = p[1];
        }
        break;
    case PROTO_MSG_USAGE:
        if (len >= 1) {
            uint8_t n = p[0];
            for (int i = 0; i < n && 1 + i * 2 + 1 < len; i++) {
                uint8_t slot = p[1 + i * 2], pct = p[2 + i * 2];
                if (slot < USAGE_SLOTS) g_app.usage_pct[slot] = pct;
            }
        }
        break;
    case PROTO_MSG_CONTEXT:
        if (len >= 9) {
            memcpy(&g_app.ctx_used, p, 4);
            memcpy(&g_app.ctx_total, p + 4, 4);
            uint8_t n = p[8];
            memset(g_app.ctx_cat_tokens, 0, sizeof(g_app.ctx_cat_tokens));
            for (int i = 0; i < n && 9 + i * 5 + 4 < len; i++) {
                uint8_t cat = p[9 + i * 5];
                if (cat < CTX_CATEGORIES)
                    memcpy(&g_app.ctx_cat_tokens[cat], p + 10 + i * 5, 4);
            }
        }
        break;
    case PROTO_MSG_CONFIG:
        if (len >= 5) {
            uint32_t v;
            memcpy(&v, p + 1, 4);
            if (p[0] == 0) g_app.brightness = (uint8_t)v;
            if (p[0] == 1 && (v == 1 || v == 4)) g_app.matrix_tiles = (uint8_t)v;
            if (p[0] == 3 && v <= 1) {
                extern esp_err_t ui_lang_set(int); /* ui/i18n.c,避免头循环 */
                ui_lang_set((int)v);
            }
            if (p[0] == 4) { /* 灯效: [15:8]=mode [7:0]=speed,颜色取 key=5 缓存 */
                extern void led_engine_set_fx(int, uint8_t, uint8_t, uint8_t, uint8_t);
                led_engine_set_fx((int)(v >> 8) & 0xFF,
                                  (g_fx_color_cache >> 16) & 0xFF,
                                  (g_fx_color_cache >> 8) & 0xFF,
                                  g_fx_color_cache & 0xFF, v & 0xFF);
            }
            if (p[0] == 5) g_fx_color_cache = v; /* 0xRRGGBB,先于 key=4 下发 */
        }
        break;
    default:
        break;
    }
    taskEXIT_CRITICAL(&s_lock);

    /* 临界区外处理带副作用的消息 */
    if (type == PROTO_MSG_TEXT && len >= 2) {
        app_text_push(p[0], p[1], (const char *)p + 2, len - 2);
    } else if (type == PROTO_MSG_TONE && len >= 1) {
        extern void audio_play_tone(uint8_t tone_id); /* audio/audio.c */
        audio_play_tone(p[0]);
    }
    ESP_LOGD(TAG, "frame 0x%02x len=%u state=%d", type, len, g_app.state);
}
