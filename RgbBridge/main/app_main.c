#include "esp_log.h"

#include "link_spi.h"
#include "patterns.h"
#include "rgb_panel.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "RgbBridge boot: SPI/i80 → ST7701S 480x480 RGB565");

    ESP_ERROR_CHECK(rgb_panel_init()); /* 先点亮面板 */
    ESP_ERROR_CHECK(patterns_start()); /* 自测图案(不接主机也能看到屏亮) */
    ESP_ERROR_CHECK(link_spi_start()); /* 接收主机帧;来帧后自测让位 */

    ESP_LOGI(TAG, "up");
}
