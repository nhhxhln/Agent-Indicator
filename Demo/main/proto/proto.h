/* 协议帧解析/构造 — 与 host/agentind/protocol.py 严格对应。
 * 帧:0xA9 | VER | TYPE | FLAGS | LEN(LE16) | PAYLOAD | CRC16(LE)
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define PROTO_MAGIC        0xA9
#define PROTO_VERSION      0x01
#define PROTO_MAX_PAYLOAD  512
#define PROTO_OVERHEAD     8 /* magic+ver+type+flags+len2+crc2 */

enum {
    PROTO_MSG_STATE     = 0x01,
    PROTO_MSG_USAGE     = 0x02,
    PROTO_MSG_CONTEXT   = 0x03,
    PROTO_MSG_TEXT      = 0x04,
    PROTO_MSG_TONE      = 0x05,
    PROTO_MSG_CONFIG    = 0x06,
    PROTO_MSG_TELEMETRY = 0x80,
    PROTO_MSG_INPUT     = 0x81,
    PROTO_MSG_MIC_LEVEL = 0x82,
};

typedef void (*proto_frame_cb_t)(uint8_t type, uint8_t flags,
                                 const uint8_t *payload, uint16_t len, void *ctx);

/* 流式解析器(粘包安全),每条链路各持一个实例 */
typedef struct {
    uint8_t  buf[PROTO_MAX_PAYLOAD + PROTO_OVERHEAD];
    uint16_t pos;
} proto_parser_t;

void     proto_parser_reset(proto_parser_t *p);
void     proto_parser_feed(proto_parser_t *p, const uint8_t *data, size_t len,
                           proto_frame_cb_t cb, void *ctx);
/* 构造帧,返回总长;out 至少 payload_len + PROTO_OVERHEAD */
size_t   proto_build(uint8_t *out, uint8_t type, const uint8_t *payload, uint16_t len);
uint16_t proto_crc16(const uint8_t *data, size_t len);
