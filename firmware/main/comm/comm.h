/* 三链路通讯:统一上行入口(app_state_apply_frame)+ 活动链路回传遥测。 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    COMM_LINK_NONE = 0,
    COMM_LINK_WIFI,
    COMM_LINK_CAN,
    COMM_LINK_USB,
} comm_link_t;

esp_err_t comm_wifi_start(void);  /* STA 接入 PC AP + WS server + mDNS */
esp_err_t comm_twai_start(void);  /* 500kbps,ID 0x500/0x580 段协议 */
esp_err_t comm_usb_start(void);   /* TinyUSB vendor bulk EP + CDC 日志 */

/* comm 各实现收到有效帧时调用:更新活动链路并转 app_state */
void comm_on_frame(comm_link_t link, uint8_t type, const uint8_t *payload, uint16_t len);
/* 设备→host:经活动链路发送(payload 为协议帧 payload,内部各自封装) */
void comm_send(uint8_t type, const uint8_t *payload, uint16_t len);
comm_link_t comm_active_link(void);
