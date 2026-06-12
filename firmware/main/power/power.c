#include "power.h"

#include <string.h>

#include "comm/comm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "proto/proto.h"
#include "ui/screens/screens.h"

static const char *TAG = "power";

#define REPORT_PERIOD_MS 2000

static void power_task(void *arg)
{
    while (1) {
        /* TODO 回板后实装:
         *  - INA226 读 VSYS 电压/电流(I2C 0x40)
         *  - MP2760 读充电状态/输入源(I2C 0x5C)
         *  - 电池 3 节分压 ADC(经均衡抽头)→ 单节电压
         *  - SOC:开路电压查表 + 电流积分修正 */
        uint8_t payload[11];
        uint16_t cell[3] = { 3700, 3700, 3700 }; /* 骨架占位值 */
        int16_t ibat = 0;
        memcpy(payload, cell, 6);
        memcpy(payload + 6, &ibat, 2);
        payload[8] = 50;  /* soc % */
        payload[9] = 0;   /* chg_state */
        payload[10] = 0;  /* power_src */
        if (comm_active_link() != COMM_LINK_NONE)
            comm_send(PROTO_MSG_TELEMETRY, payload, sizeof(payload));
        ui_devices_set_power(cell[0] + cell[1] + cell[2], ibat, payload[8]);
        vTaskDelay(pdMS_TO_TICKS(REPORT_PERIOD_MS));
    }
}

esp_err_t power_start(void)
{
    xTaskCreate(power_task, "power", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "power telemetry every %dms", REPORT_PERIOD_MS);
    return ESP_OK;
}
