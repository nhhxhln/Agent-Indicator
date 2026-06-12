/* CAN 测试 case:
 *   can_tx [count] [period_ms]  周期发送递增计数帧(ID 0x123),后台任务
 *   can_rx [on|off]             打印总线上收到的所有帧(监听任务)
 *   can_status                  TWAI 控制器状态/错误计数
 *
 * PC 侧对测:
 *   candump can0
 *   cangen can0 -I 321 -L 8 -g 100
 *
 * 注意:comm_twai.c 的 rx_task 只消费 0x500-0x51F 协议帧;本 case 的监听
 * 通过 twai_receive 与其竞争,联调协议时请 can_rx off。 */
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "console/app_console.h"
#include "driver/twai.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "case_can";

namespace {

constexpr uint32_t kTxId = 0x123;
constexpr int kTaskStack = 4096; /* 纯驱动调用 + printf,4K 足够 */

TaskHandle_t tx_task_handle = nullptr;
TaskHandle_t rx_task_handle = nullptr;
struct { int count; int period_ms; } tx_args;

void tx_task(void *)
{
    uint32_t seq = 0;
    int remaining = tx_args.count;
    while (remaining != 0) { /* count<0 表示无限 */
        twai_message_t msg = {};
        msg.identifier = kTxId;
        msg.data_length_code = 8;
        memcpy(msg.data, &seq, 4);
        uint32_t tick = xTaskGetTickCount();
        memcpy(msg.data + 4, &tick, 4);
        esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(100));
        if (err == ESP_OK)
            ESP_LOGI(TAG, "tx seq=%lu", (unsigned long)seq);
        else
            ESP_LOGW(TAG, "tx failed: %s", esp_err_to_name(err));
        seq++;
        if (remaining > 0) remaining--;
        vTaskDelay(pdMS_TO_TICKS(tx_args.period_ms));
    }
    tx_task_handle = nullptr;
    vTaskDelete(nullptr);
}

void rx_task(void *)
{
    twai_message_t msg;
    while (true) {
        if (twai_receive(&msg, portMAX_DELAY) != ESP_OK) continue;
        char hex[3 * 8 + 1] = {};
        for (int i = 0; i < msg.data_length_code; i++)
            snprintf(hex + i * 3, 4, "%02X ", msg.data[i]);
        ESP_LOGI(TAG, "rx id=0x%03lX dlc=%d [%s]",
                 (unsigned long)msg.identifier, msg.data_length_code, hex);
    }
}

int cmd_can_tx(int argc, char **argv)
{
    if (tx_task_handle) {
        printf("tx already running\n");
        return 1;
    }
    tx_args.count = argc >= 2 ? atoi(argv[1]) : 10;
    tx_args.period_ms = argc >= 3 ? atoi(argv[2]) : 100;
    xTaskCreate(tx_task, "can_tx", kTaskStack, nullptr, 9, &tx_task_handle);
    printf("tx started: count=%d period=%dms id=0x%03lX\n",
           tx_args.count, tx_args.period_ms, (unsigned long)kTxId);
    return 0;
}

int cmd_can_rx(int argc, char **argv)
{
    bool on = argc < 2 || strcmp(argv[1], "on") == 0;
    if (on && !rx_task_handle) {
        xTaskCreate(rx_task, "can_rx", kTaskStack, nullptr, 9, &rx_task_handle);
        printf("rx monitor on\n");
    } else if (!on && rx_task_handle) {
        vTaskDelete(rx_task_handle);
        rx_task_handle = nullptr;
        printf("rx monitor off\n");
    }
    return 0;
}

int cmd_can_status(int, char **)
{
    twai_status_info_t st;
    if (twai_get_status_info(&st) != ESP_OK) {
        printf("twai not installed\n");
        return 1;
    }
    printf("state=%d tx_q=%lu rx_q=%lu tx_err=%lu rx_err=%lu bus_err=%lu\n",
           st.state, (unsigned long)st.msgs_to_tx, (unsigned long)st.msgs_to_rx,
           (unsigned long)st.tx_error_counter, (unsigned long)st.rx_error_counter,
           (unsigned long)st.bus_error_count);
    return 0;
}

} // namespace

extern "C" void case_can_register(void)
{
    const esp_console_cmd_t cmds[] = {
        { .command = "can_tx", .help = "can_tx [count] [period_ms] - 发送测试帧",
          .hint = nullptr, .func = cmd_can_tx, .argtable = nullptr },
        { .command = "can_rx", .help = "can_rx [on|off] - 总线监听",
          .hint = nullptr, .func = cmd_can_rx, .argtable = nullptr },
        { .command = "can_status", .help = "TWAI 状态与错误计数",
          .hint = nullptr, .func = cmd_can_status, .argtable = nullptr },
    };
    for (auto &c : cmds) ESP_ERROR_CHECK(esp_console_cmd_register(&c));
}
