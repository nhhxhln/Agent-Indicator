/* 全局状态模型:comm 层写入,ui/audio 任务读取渲染。 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AGENT_IDLE = 0,
    AGENT_CONNECTING,
    AGENT_THINKING,
    AGENT_RESPONDING,
    AGENT_TOOL_USE,
    AGENT_WAITING_USER,
    AGENT_ERROR,
    AGENT_RATE_LIMITED,
    AGENT_OFFLINE,
    AGENT_STATE_MAX,
} agent_state_t;

#define USAGE_SLOTS     4
#define CTX_CATEGORIES  6
#define TEXT_RING_SIZE  2048

typedef struct {
    agent_state_t state;
    uint8_t       state_detail;
    uint8_t       usage_pct[USAGE_SLOTS];     /* 0..100,0xFF = 未上报 */
    uint32_t      ctx_used;
    uint32_t      ctx_total;
    uint32_t      ctx_cat_tokens[CTX_CATEGORIES];
    uint8_t       mic_level_db;               /* 本地拾音 VU,audio 任务写 */
    uint8_t       brightness;                 /* 0..255 全局亮度 */
    uint8_t       matrix_tiles;               /* 1 或 4 */
    int64_t       last_rx_us;                 /* 最近一次 host 帧时间,判离线 */
} app_state_t;

extern app_state_t g_app;

void app_state_init(void);
/* comm 层收到协议帧后调用;线程安全(内部自旋锁保护) */
void app_state_apply_frame(uint8_t type, const uint8_t *payload, uint16_t len);
/* 文本流:display 任务消费 */
void app_text_push(uint8_t stream, uint8_t op, const char *utf8, uint16_t len);
int  app_text_pop(char *buf, int maxlen, uint8_t *stream);

#ifdef __cplusplus
}
#endif
