#include "rgb_bridge_host.h"

#include <string.h>

#include "board.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"

static const char *TAG = "rgb_host";

#define W       PANEL_W
#define H       PANEL_H
#define HDR     4
#define PKT_MAX (HDR + W * 3)
#define HOST    SPI2_HOST

static spi_device_handle_t s_dev;
static uint8_t *s_pkt; /* DMA 发送缓冲(内部 RAM) */

esp_err_t rgb_bridge_host_init(int clk_hz)
{
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PKT_MAX,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(HOST, &bus, SPI_DMA_CH_AUTO), TAG, "bus init");

    spi_device_interface_config_t dev = {
        .clock_speed_hz = clk_hz,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(HOST, &dev, &s_dev), TAG, "add dev");

    s_pkt = heap_caps_malloc(PKT_MAX, MALLOC_CAP_DMA);
    return s_pkt ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t rgb_bridge_push_line(int y, int fmt, const void *line)
{
    int bpp = (fmt == RGB_FMT_888) ? 3 : 2;
    s_pkt[0] = 0xA9;
    s_pkt[1] = (uint8_t)fmt;
    s_pkt[2] = (uint8_t)(y & 0xFF);
    s_pkt[3] = (uint8_t)((y >> 8) & 0xFF);
    memcpy(s_pkt + HDR, line, (size_t)W * bpp);

    spi_transaction_t t = { 0 };
    t.length = (size_t)(HDR + W * bpp) * 8;
    t.tx_buffer = s_pkt;
    return spi_device_transmit(s_dev, &t);
}

esp_err_t rgb_bridge_push_frame(const void *fb, int fmt)
{
    int bpp = (fmt == RGB_FMT_888) ? 3 : 2;
    const uint8_t *src = (const uint8_t *)fb;
    for (int y = 0; y < H; y++) {
        esp_err_t e = rgb_bridge_push_line(y, fmt, src + (size_t)y * W * bpp);
        if (e != ESP_OK) return e;
    }
    return ESP_OK;
}
