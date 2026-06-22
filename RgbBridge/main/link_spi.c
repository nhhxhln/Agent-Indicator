#include "link_spi.h"

#include <string.h>

#include "board.h"
#include "driver/spi_slave.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "patterns.h" /* g_host_active */
#include "rgb_panel.h"

static const char *TAG = "link_spi";

#define W        PANEL_H_RES
#define H        PANEL_V_RES
#define HDR      4
#define PKT_MAX  (HDR + W * 3) /* 一行最大:888 时 4 + 480*3 = 1444 字节 */
#define HOST     SPI2_HOST

volatile bool g_host_active = false;

static uint16_t *s_stage; /* 整帧 565 暂存(PSRAM) */
static uint8_t  *s_rx;    /* DMA 接收缓冲(内部 RAM) */

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void link_task(void *arg)
{
    while (1) {
        spi_slave_transaction_t t = { 0 };
        t.length = PKT_MAX * 8;
        t.rx_buffer = s_rx;
        if (spi_slave_transmit(HOST, &t, portMAX_DELAY) != ESP_OK) continue;

        int n = (int)(t.trans_len / 8);
        if (n < HDR || s_rx[0] != 0xA9) continue; /* 帧同步:丢弃错位包 */
        uint8_t fmt = s_rx[1];
        uint16_t y = (uint16_t)(s_rx[2] | (s_rx[3] << 8));
        if (y >= H) continue;

        uint16_t *dst = &s_stage[(size_t)y * W];
        const uint8_t *p = s_rx + HDR;
        if (fmt == 0) {                       /* RGB565 小端,直拷 */
            memcpy(dst, p, (size_t)W * 2);
        } else {                              /* RGB888 → 565 */
            for (int x = 0; x < W; x++)
                dst[x] = rgb565(p[x * 3], p[x * 3 + 1], p[x * 3 + 2]);
        }

        if (!g_host_active) {
            g_host_active = true;
            ESP_LOGI(TAG, "host stream started (fmt=%s)", fmt ? "RGB888" : "RGB565");
        }
        if (y == H - 1) rgb_panel_blit_full(s_stage); /* 收满一帧上屏 */
    }
}

esp_err_t link_spi_start(void)
{
    s_stage = heap_caps_malloc((size_t)W * H * 2, MALLOC_CAP_SPIRAM);
    s_rx = heap_caps_malloc(PKT_MAX, MALLOC_CAP_DMA);
    if (!s_stage || !s_rx) {
        ESP_LOGE(TAG, "alloc fail");
        return ESP_ERR_NO_MEM;
    }
    memset(s_stage, 0, (size_t)W * H * 2);

    spi_bus_config_t bus = {
        .mosi_io_num = HOST_SPI_MOSI,
        .miso_io_num = HOST_SPI_MISO,
        .sclk_io_num = HOST_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PKT_MAX,
    };
    spi_slave_interface_config_t slv = {
        .mode = 0,
        .spics_io_num = HOST_SPI_CS,
        .queue_size = 3,
    };
    ESP_RETURN_ON_ERROR(spi_slave_initialize(HOST, &bus, &slv, SPI_DMA_CH_AUTO),
                        TAG, "spi_slave_initialize");

    xTaskCreatePinnedToCore(link_task, "spilink", 4096, NULL, 6, NULL, 1);
    ESP_LOGI(TAG, "SPI slave up: cs=%d clk=%d mosi=%d (一行包 ≤%dB)",
             HOST_SPI_CS, HOST_SPI_CLK, HOST_SPI_MOSI, PKT_MAX);
    return ESP_OK;
}
