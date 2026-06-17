/* Agent Indicator 固件入口。
 * 启动顺序:状态模型 → I2C/存储(基础设施)→ LED(最先有视觉反馈)
 * → 显示 → 音频 → 三链路 → 遥测 → 测试控制台。 */
#include "app_state.h"
#include "audio/audio.h"
#include "bus/i2c_bus.h"
#include "comm/comm.h"
#include "console/app_console.h"
#include "drivers/sensors.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "power/power.h"
#include "ui/i18n.h"
#include "storage/storage.h"
#include "ui/display.h"
#include "ui/led_engine.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Agent Indicator boot");
    app_state_init();

    ESP_ERROR_CHECK(nvs_flash_init()); /* comm_wifi 内重复调用无害 */
    ESP_ERROR_CHECK(i2c_bus_init());
    sensors_init();
    ESP_ERROR_CHECK(storage_init());
    ui_i18n_init();

    ESP_ERROR_CHECK(led_engine_start());
    ESP_ERROR_CHECK(display_start());
    ESP_ERROR_CHECK(audio_start());

    ESP_ERROR_CHECK(comm_usb_start());
#if CONFIG_AGENTIND_ENABLE_CAN
    ESP_ERROR_CHECK(comm_twai_start()); /* DevKitC-1 引脚冲突,默认禁用 */
#endif
    /* Wi-Fi 暂时关闭:真机报 AES/中断分配不足(LEVEL 中断耗尽),先排音频。
     * 需要时取消注释即可恢复。 */
    /* ESP_ERROR_CHECK(comm_wifi_start()); */ /* 含 nvs/netif/event loop 初始化 */

    ESP_ERROR_CHECK(power_start());
    ESP_ERROR_CHECK(app_console_start());

    audio_play_tone(3); /* boot 提示音 */
    ESP_LOGI(TAG, "all subsystems up");
}
