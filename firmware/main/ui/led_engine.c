#include "led_engine.h"

#include <math.h>
#include <string.h>

#include "app_state.h"
#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "led";

#define FPS             60
#define MATRIX_MAX      (BOARD_MATRIX_W * BOARD_MATRIX_H * 4) /* 最多 4 块 */
#define AUX_LEDS        (BOARD_RING_LEDS + BOARD_USAGE_LEDS + BOARD_VU_LEDS)
#define AUX_OFF_RING    0
#define AUX_OFF_USAGE   BOARD_RING_LEDS
#define AUX_OFF_VU      (BOARD_RING_LEDS + BOARD_USAGE_LEDS)
#define MA_PER_CHANNEL  16 /* WS2812B 每色全亮电流估算 */

typedef struct { uint8_t r, g, b; } rgb_t;

static led_strip_handle_t s_matrix, s_aux;
static rgb_t s_mbuf[MATRIX_MAX];
static rgb_t s_abuf[AUX_LEDS];

/* 每状态主题色 */
static const rgb_t STATE_COLOR[AGENT_STATE_MAX] = {
    [AGENT_IDLE]         = { 16, 16, 24 },
    [AGENT_CONNECTING]   = { 0, 32, 64 },
    [AGENT_THINKING]     = { 90, 40, 160 },  /* 紫:思考 */
    [AGENT_RESPONDING]   = { 0, 140, 90 },   /* 绿:输出 */
    [AGENT_TOOL_USE]     = { 200, 110, 0 },  /* 橙:工具 */
    [AGENT_WAITING_USER] = { 0, 90, 180 },   /* 蓝:等待 */
    [AGENT_ERROR]        = { 180, 10, 10 },
    [AGENT_RATE_LIMITED] = { 150, 60, 0 },
    [AGENT_OFFLINE]      = { 8, 8, 8 },
};

static const rgb_t CTX_COLOR[CTX_CATEGORIES] = {
    { 60, 60, 70 },   /* system */
    { 200, 110, 0 },  /* tools  */
    { 0, 120, 160 },  /* mcp    */
    { 130, 0, 160 },  /* memory */
    { 0, 150, 80 },   /* messages */
    { 4, 4, 6 },      /* free   */
};

static inline rgb_t scale(rgb_t c, uint8_t k)
{
    return (rgb_t){ c.r * k / 255, c.g * k / 255, c.b * k / 255 };
}

/* ---- 覆盖灯效(OpenRGB 风格预设) ---- */
static volatile led_fx_t s_fx = LED_FX_AGENT;
static rgb_t s_fx_color = { 0, 140, 255 };
static uint8_t s_fx_speed = 40; /* 1-100 */

void led_engine_set_fx(led_fx_t fx, uint8_t r, uint8_t g, uint8_t b, uint8_t speed)
{
    if (fx >= LED_FX_MAX) return;
    s_fx_color = (rgb_t){ r, g, b };
    if (speed >= 1 && speed <= 100) s_fx_speed = speed;
    s_fx = fx;
}

led_fx_t led_engine_get_fx(void) { return s_fx; }

static rgb_t hue_to_rgb(uint8_t h) /* HSV 色环,S=V=255 */
{
    uint8_t seg = h / 43, rem = (h - seg * 43) * 6;
    uint8_t q = 255 - rem, t_ = rem;
    switch (seg) {
    case 0: return (rgb_t){ 255, t_, 0 };
    case 1: return (rgb_t){ q, 255, 0 };
    case 2: return (rgb_t){ 0, 255, t_ };
    case 3: return (rgb_t){ 0, q, 255 };
    case 4: return (rgb_t){ t_, 0, 255 };
    default: return (rgb_t){ 255, 0, q };
    }
}

/* 将 matrix+aux 视作一条逻辑灯带统一渲染覆盖灯效 */
static void render_fx(float t, int mleds)
{
    int total = mleds + AUX_LEDS;
    float spd = s_fx_speed / 40.0f;
    for (int i = 0; i < total; i++) {
        rgb_t c = { 0, 0, 0 };
        switch (s_fx) {
        case LED_FX_SOLID:
            c = s_fx_color;
            break;
        case LED_FX_BREATH: {
            float k = 0.12f + 0.88f * 0.5f * (1 + sinf(t * 2.0f * spd));
            c = scale(s_fx_color, (uint8_t)(k * 255));
            break;
        }
        case LED_FX_MARQUEE: {
            int pos = (int)(t * 24 * spd);
            c = (((i - pos) % 12 + 12) % 12) < 4 ? s_fx_color
                                                 : scale(s_fx_color, 18);
            break;
        }
        case LED_FX_RAINBOW:
            c = hue_to_rgb((uint8_t)(i * 256 / total + (int)(t * 64 * spd)));
            break;
        default: /* OFF */
            break;
        }
        if (i < mleds) s_mbuf[i] = c;
        else s_abuf[i - mleds] = c;
    }
}

/* matrix 物理映射:tile 内 S 形扫描,4 块时 2×2 排列(Z 序),链序 = tile0..3 */
static int matrix_index(int x, int y, int tiles)
{
    int w = BOARD_MATRIX_W;
    int tile = 0, tx = x, ty = y;
    if (tiles == 4) {
        tile = (y >= w ? 2 : 0) + (x >= w ? 1 : 0);
        tx = x % w;
        ty = y % w;
    }
    int in_tile = (ty & 1) ? ty * w + (w - 1 - tx) : ty * w + tx; /* serpentine */
    return tile * w * w + in_tile;
}

/* ------------------------------------------------ renderers (every frame) */

static void render_ring(float t)
{
    rgb_t c = STATE_COLOR[g_app.state];
    for (int i = 0; i < BOARD_RING_LEDS; i++) {
        float k = 0;
        switch (g_app.state) {
        case AGENT_THINKING: { /* 旋转彗星 */
            float pos = fmodf(t * 1.5f, 1.0f) * BOARD_RING_LEDS;
            float d = fmodf(i - pos + BOARD_RING_LEDS, BOARD_RING_LEDS);
            k = expf(-d * 0.45f);
            break;
        }
        case AGENT_RESPONDING: /* 整环流光 */
            k = 0.55f + 0.45f * sinf(t * 6 + i * (2 * M_PI / BOARD_RING_LEDS) * 3);
            break;
        case AGENT_TOOL_USE: /* 双彗星对转 */ {
            float p1 = fmodf(t * 2.0f, 1.0f) * BOARD_RING_LEDS;
            float p2 = BOARD_RING_LEDS - p1;
            float d1 = fmodf(i - p1 + BOARD_RING_LEDS, BOARD_RING_LEDS);
            float d2 = fmodf(i - p2 + BOARD_RING_LEDS, BOARD_RING_LEDS);
            k = fmaxf(expf(-d1 * 0.5f), expf(-d2 * 0.5f));
            break;
        }
        case AGENT_WAITING_USER: /* 慢呼吸 */
        case AGENT_IDLE:
            k = 0.25f + 0.75f * 0.5f * (1 + sinf(t * (g_app.state == AGENT_IDLE ? 1.2f : 2.5f)));
            break;
        case AGENT_ERROR: /* 闪烁 */
            k = (fmodf(t, 0.5f) < 0.25f) ? 1.0f : 0.05f;
            break;
        default:
            k = 0.3f;
        }
        s_abuf[AUX_OFF_RING + i] = scale(c, (uint8_t)(k * 255));
    }
}

static void render_usage(float t)
{
    /* 20 LED 分两段:0-9 = SESSION,10-19 = LIMIT_5H;未上报则呼吸灰 */
    const int seg = BOARD_USAGE_LEDS / 2;
    for (int s = 0; s < 2; s++) {
        uint8_t pct = g_app.usage_pct[s];
        for (int i = 0; i < seg; i++) {
            rgb_t c = { 6, 6, 8 };
            if (pct != 0xFF) {
                float fill = pct / 100.0f * seg;
                if (i < (int)fill) {
                    /* 绿→黄→红 渐变 */
                    uint8_t r = i * 255 / seg, g = 255 - r;
                    c = (rgb_t){ r, g, 20 };
                } else if (i < fill + 1 && fmodf(t * 2, 1.0f) < 0.5f) {
                    c = (rgb_t){ 40, 40, 40 }; /* 尖端闪烁 */
                }
            }
            s_abuf[AUX_OFF_USAGE + s * seg + i] = c;
        }
    }
}

static void render_vu(void)
{
    /* 拾音条:中心向两侧展开,g_app.mic_level_db 由 audio 任务写入(0..60) */
    int half = BOARD_VU_LEDS / 2;
    int lit = g_app.mic_level_db * half / 60;
    for (int i = 0; i < half; i++) {
        rgb_t c = (i < lit) ? (rgb_t){ 0, 90 + i * 2, 140 } : (rgb_t){ 2, 2, 4 };
        s_abuf[AUX_OFF_VU + half - 1 - i] = c;
        s_abuf[AUX_OFF_VU + half + i] = c;
    }
}

static void render_matrix(float t)
{
    int tiles = g_app.matrix_tiles == 4 ? 4 : 1;
    int dim = BOARD_MATRIX_W * (tiles == 4 ? 2 : 1);
    int cells = dim * dim;
    uint32_t total = g_app.ctx_total ? g_app.ctx_total : 1;

    /* 按类别顺序填充热力图,行优先;尾部为 free */
    int cell = 0;
    memset(s_mbuf, 0, sizeof(s_mbuf));
    for (int cat = 0; cat < CTX_CATEGORIES && cell < cells; cat++) {
        int n = (int)((int64_t)g_app.ctx_cat_tokens[cat] * cells / total);
        for (int j = 0; j < n && cell < cells; j++, cell++) {
            int x = cell % dim, y = cell / dim;
            s_mbuf[matrix_index(x, y, tiles)] = CTX_COLOR[cat];
        }
    }
    /* 已用未分类部分 */
    int used_cells = (int)((int64_t)g_app.ctx_used * cells / total);
    for (; cell < used_cells && cell < cells; cell++) {
        int x = cell % dim, y = cell / dim;
        s_mbuf[matrix_index(x, y, tiles)] = (rgb_t){ 50, 50, 60 };
    }
    /* 用量前沿呼吸 */
    if (used_cells > 0 && used_cells <= cells) {
        int x = (used_cells - 1) % dim, y = (used_cells - 1) / dim;
        uint8_t k = 128 + 127 * sinf(t * 4);
        s_mbuf[matrix_index(x, y, tiles)] = scale((rgb_t){ 255, 255, 255 }, k);
    }
}

/* 全局限流:折算总电流超预算时整体等比降亮 */
static float current_limit_factor(int matrix_leds)
{
    uint32_t ma = 0;
    for (int i = 0; i < matrix_leds; i++)
        ma += (s_mbuf[i].r + s_mbuf[i].g + s_mbuf[i].b) * MA_PER_CHANNEL / 255;
    for (int i = 0; i < AUX_LEDS; i++)
        ma += (s_abuf[i].r + s_abuf[i].g + s_abuf[i].b) * MA_PER_CHANNEL / 255;
    uint32_t budget = CONFIG_AGENTIND_VLED_BUDGET_MA;
    return (ma > budget) ? (float)budget / ma : 1.0f;
}

static void led_task(void *arg)
{
    TickType_t wake = xTaskGetTickCount();
    float t = 0;
    while (1) {
        int mleds = BOARD_MATRIX_W * BOARD_MATRIX_H * g_app.matrix_tiles;
        if (s_fx == LED_FX_AGENT) {
            render_ring(t);
            render_usage(t);
            render_vu();
            render_matrix(t);
        } else {
            render_fx(t, mleds);
        }
        float lim = current_limit_factor(mleds) * g_app.brightness / 255.0f;

        for (int i = 0; i < mleds; i++) {
            rgb_t c = scale(s_mbuf[i], (uint8_t)(lim * 255));
            led_strip_set_pixel(s_matrix, i, c.r, c.g, c.b);
        }
        for (int i = 0; i < AUX_LEDS; i++) {
            rgb_t c = scale(s_abuf[i], (uint8_t)(lim * 255));
            led_strip_set_pixel(s_aux, i, c.r, c.g, c.b);
        }
        led_strip_refresh(s_matrix);
        led_strip_refresh(s_aux);

        t += 1.0f / FPS;
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1000 / FPS));
    }
}

esp_err_t led_engine_start(void)
{
    led_strip_config_t mc = {
        .strip_gpio_num = BOARD_LED_MATRIX_GPIO,
        .max_leds = MATRIX_MAX,
        .led_model = LED_MODEL_WS2812,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
    };
    led_strip_rmt_config_t rc = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = true, /* S3 支持,长链稳定 */
    };
    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&mc, &rc, &s_matrix), TAG, "matrix");

    led_strip_config_t ac = mc;
    ac.strip_gpio_num = BOARD_LED_AUX_GPIO;
    ac.max_leds = AUX_LEDS;
    led_strip_rmt_config_t ac_rmt = rc;
    ac_rmt.flags.with_dma = false; /* S3 仅一路 RMT-DMA */
    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&ac, &ac_rmt, &s_aux), TAG, "aux");

    xTaskCreatePinnedToCore(led_task, "led", 6144, NULL, 8, NULL, 1);
    ESP_LOGI(TAG, "led engine @%dfps, budget %dmA", FPS, CONFIG_AGENTIND_VLED_BUDGET_MA);
    return ESP_OK;
}
