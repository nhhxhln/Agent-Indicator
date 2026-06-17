/* USB 链路:TinyUSB 复合设备 = Vendor class(bulk EP,协议帧)+ CDC(日志)。
 * VID:PID = 303a:82a9,与 host/agentind/transports/usb.py 对应。
 *
 * 骨架说明:esp_tinyusb 默认描述符不含 vendor class,需在 menuconfig 打开
 * TinyUSB vendor 支持并提供自定义描述符;本文件给出回调与收发路径,
 * 描述符 TODO 标注待硬件回板后补全。 */
#include "comm.h"

#include "esp_log.h"
#include "proto/proto.h"

static const char *TAG = "comm_usb";

#if CONFIG_TINYUSB_VENDOR_ENABLED /* TODO: 自定义描述符后启用 */

#include "tinyusb.h"
#include "tusb.h"

static proto_parser_t s_parser;

static void on_frame_cb(uint8_t type, uint8_t flags, const uint8_t *payload,
                        uint16_t len, void *ctx)
{
    (void)flags; (void)ctx;
    comm_on_frame(COMM_LINK_USB, type, payload, len);
}

/* TinyUSB vendor 收包回调 */
void tud_vendor_rx_cb(uint8_t itf, const uint8_t *buffer, uint16_t bufsize)
{
    (void)itf;
    proto_parser_feed(&s_parser, buffer, bufsize, on_frame_cb, NULL);
    tud_vendor_read_flush();
}

void comm_usb_send(uint8_t type, const uint8_t *payload, uint16_t len)
{
    uint8_t buf[PROTO_MAX_PAYLOAD + PROTO_OVERHEAD];
    size_t n = proto_build(buf, type, payload, len);
    tud_vendor_write(buf, n);
    tud_vendor_write_flush();
}

esp_err_t comm_usb_start(void)
{
    proto_parser_reset(&s_parser);
    const tinyusb_config_t cfg = {
        /* TODO: .device_descriptor / .configuration_descriptor 填入
         * vendor(EP 0x01/0x81)+ CDC 复合描述符,PID 0x82A9 */
    };
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&cfg), TAG, "tusb install");
    ESP_LOGI(TAG, "usb vendor+cdc up");
    return ESP_OK;
}

#else /* 未启用 vendor class 时的占位实现,Wi-Fi/CAN 不受影响 */

void comm_usb_send(uint8_t type, const uint8_t *payload, uint16_t len)
{
    (void)type; (void)payload; (void)len;
}

esp_err_t comm_usb_start(void)
{
    ESP_LOGW(TAG, "usb link disabled (CONFIG_TINYUSB_VENDOR_ENABLED not set)");
    return ESP_OK;
}

#endif
