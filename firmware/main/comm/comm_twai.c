/* CAN/TWAI 链路:500kbps。映射与 host/agentind/transports/can.py 对应:
 *   host→dev ID = 0x500|TYPE5,dev→host ID = 0x580|TYPE5
 *   分段:byte0 = (seq<<4)|total,首段 byte1 = TYPE 原值 */
#include <string.h>

#include "comm.h"

#include "board.h"
#include "driver/twai.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "proto/proto.h"

static const char *TAG = "comm_twai";

#define ID_H2D_BASE 0x500
#define ID_D2H_BASE 0x580

static uint8_t s_reasm[PROTO_MAX_PAYLOAD + 1];
static uint16_t s_reasm_len;
static int8_t s_expect_seq = -1;

static void rx_task(void *arg)
{
    twai_message_t msg;
    while (1) {
        if (twai_receive(&msg, portMAX_DELAY) != ESP_OK) continue;
        if (msg.identifier < ID_H2D_BASE || msg.identifier >= ID_H2D_BASE + 0x20 ||
            msg.data_length_code < 2)
            continue;
        uint8_t seq = msg.data[0] >> 4, total = msg.data[0] & 0x0F;
        if (seq == 0) { s_reasm_len = 0; s_expect_seq = 0; }
        if (seq != s_expect_seq) { s_expect_seq = -1; continue; } /* 丢段,放弃本条 */
        uint16_t n = msg.data_length_code - 1;
        if (s_reasm_len + n <= sizeof(s_reasm)) {
            memcpy(s_reasm + s_reasm_len, msg.data + 1, n);
            s_reasm_len += n;
        }
        if (++s_expect_seq >= total) {
            s_expect_seq = -1;
            if (s_reasm_len >= 1)
                comm_on_frame(COMM_LINK_CAN, s_reasm[0], s_reasm + 1, s_reasm_len - 1);
        }
    }
}

void comm_twai_send(uint8_t type, const uint8_t *payload, uint16_t len)
{
    uint8_t raw[PROTO_MAX_PAYLOAD + 1];
    raw[0] = type;
    memcpy(raw + 1, payload, len);
    uint16_t total_len = len + 1;
    uint8_t total = (total_len + 6) / 7;
    if (total > 15) { ESP_LOGW(TAG, "msg too long for CAN"); return; }
    for (uint8_t seq = 0; seq < total; seq++) {
        twai_message_t msg = {
            .identifier = ID_D2H_BASE | (type & 0x1F),
            .data_length_code = 0,
        };
        msg.data[0] = (seq << 4) | total;
        uint16_t off = seq * 7;
        uint16_t n = total_len - off < 7 ? total_len - off : 7;
        memcpy(msg.data + 1, raw + off, n);
        msg.data_length_code = n + 1;
        twai_transmit(&msg, pdMS_TO_TICKS(20));
    }
}

esp_err_t comm_twai_start(void)
{
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        BOARD_TWAI_TX, BOARD_TWAI_RX, TWAI_MODE_NORMAL);
    twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_RETURN_ON_ERROR(twai_driver_install(&g, &t, &f), TAG, "install");
    ESP_RETURN_ON_ERROR(twai_start(), TAG, "start");
    xTaskCreate(rx_task, "twai_rx", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "twai up @500k");
    return ESP_OK;
}
