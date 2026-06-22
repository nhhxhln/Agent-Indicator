#include "patterns.h"

#include <string.h>

#include "board.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rgb_panel.h"

static const char *TAG = "selftest";

#define W PANEL_H_RES
#define H PANEL_V_RES

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static uint16_t *s_fb; /* 整屏 RGB565 缓冲(PSRAM) */

static void fill_solid(uint16_t c)
{
    for (int i = 0; i < W * H; i++) s_fb[i] = c;
}

static void fill_bars(void)
{
    /* 8 条竖彩条:白黄青绿品红红蓝黑 */
    static const uint16_t bar[8] = {
        0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000,
    };
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            s_fb[y * W + x] = bar[x * 8 / W];
}

static void fill_gradient(void)
{
    /* 横向灰阶 + 纵向蓝阶,便于看坏点/偏色 */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            s_fb[y * W + x] = rgb565(x * 255 / W, x * 255 / W, y * 255 / H);
}

static void fill_checker(void)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            s_fb[y * W + x] = (((x >> 4) ^ (y >> 4)) & 1) ? 0xFFFF : 0x0000;
}

static void fill_border(uint16_t c)
{
    fill_solid(0x0000);
    for (int x = 0; x < W; x++) { s_fb[x] = c; s_fb[(H - 1) * W + x] = c; }
    for (int y = 0; y < H; y++) { s_fb[y * W] = c; s_fb[y * W + W - 1] = c; }
}

static void selftest_task(void *arg)
{
    int step = 0;
    while (1) {
        if (g_host_active) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        switch (step % 9) {
        case 0: fill_solid(0xF800); break;        /* 红 */
        case 1: fill_solid(0x07E0); break;        /* 绿 */
        case 2: fill_solid(0x001F); break;        /* 蓝 */
        case 3: fill_solid(0xFFFF); break;        /* 白 */
        case 4: fill_solid(0x0000); break;        /* 黑 */
        case 5: fill_bars(); break;               /* 彩条 */
        case 6: fill_gradient(); break;           /* 渐变 */
        case 7: fill_checker(); break;            /* 棋盘(坏点/拖影) */
        case 8: fill_border(0xFFFF); break;       /* 白边框(对位/裁切) */
        }
        rgb_panel_blit_full(s_fb);
        step++;
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
}

esp_err_t patterns_start(void)
{
    s_fb = heap_caps_malloc((size_t)W * H * 2, MALLOC_CAP_SPIRAM);
    if (!s_fb) {
        ESP_LOGE(TAG, "framebuffer alloc fail (need %d KB PSRAM)", W * H * 2 / 1024);
        return ESP_ERR_NO_MEM;
    }
    xTaskCreatePinnedToCore(selftest_task, "selftest", 4096, NULL, 4, NULL, 0);
    ESP_LOGI(TAG, "self-test running (cycling patterns; host frame will take over)");
    return ESP_OK;
}
