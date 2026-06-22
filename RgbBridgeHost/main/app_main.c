#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rgb_bridge_host.h"

static const char *TAG = "main";

#define W PANEL_W
#define H PANEL_H

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* 逐行生成"移动彩条 + 渐变"测试帧并推给桥片(不需整帧缓冲)。 */
void app_main(void)
{
    ESP_LOGI(TAG, "RgbBridgeHost: push frames over SPI master @%dMHz", LINK_CLK_HZ / 1000000);
    ESP_ERROR_CHECK(rgb_bridge_host_init(LINK_CLK_HZ));

    static uint16_t line[W]; /* 一行 565 */
    int frame = 0;
    while (1) {
        int shift = frame * 8; /* 彩条随帧移动,肉眼可见在刷新 */
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int band = ((x + shift) / 60) % 6;
                uint8_t v = (uint8_t)(y * 255 / H); /* 纵向亮度渐变 */
                uint8_t r = (band == 0 || band == 3 || band == 5) ? v : 0;
                uint8_t g = (band == 1 || band == 3 || band == 4) ? v : 0;
                uint8_t b = (band == 2 || band == 4 || band == 5) ? v : 0;
                line[x] = rgb565(r, g, b);
            }
            if (rgb_bridge_push_line(y, RGB_FMT_565, line) != ESP_OK) {
                ESP_LOGW(TAG, "push line %d failed", y);
                break;
            }
        }
        if ((frame++ & 31) == 0) ESP_LOGI(TAG, "frame %d pushed", frame);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
