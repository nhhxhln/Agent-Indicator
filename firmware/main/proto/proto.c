#include "proto.h"

#include <string.h>

uint16_t proto_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

size_t proto_build(uint8_t *out, uint8_t type, const uint8_t *payload, uint16_t len)
{
    if (len > PROTO_MAX_PAYLOAD) return 0;
    out[0] = PROTO_MAGIC;
    out[1] = PROTO_VERSION;
    out[2] = type;
    out[3] = 0; /* flags */
    out[4] = len & 0xFF;
    out[5] = len >> 8;
    if (len) memcpy(out + 6, payload, len);
    uint16_t crc = proto_crc16(out + 1, 5 + len);
    out[6 + len] = crc & 0xFF;
    out[7 + len] = crc >> 8;
    return (size_t)len + PROTO_OVERHEAD;
}

void proto_parser_reset(proto_parser_t *p) { p->pos = 0; }

static int try_extract(proto_parser_t *p, proto_frame_cb_t cb, void *ctx)
{
    /* 返回消耗的字节数;0 表示数据不足 */
    if (p->pos < 1) return 0;
    if (p->buf[0] != PROTO_MAGIC) return 1; /* 跳过垃圾字节 */
    if (p->pos < 6) return 0;
    uint16_t len = p->buf[4] | ((uint16_t)p->buf[5] << 8);
    if (p->buf[1] != PROTO_VERSION || len > PROTO_MAX_PAYLOAD) return 1;
    uint16_t total = len + PROTO_OVERHEAD;
    if (p->pos < total) return 0;
    uint16_t crc = p->buf[6 + len] | ((uint16_t)p->buf[7 + len] << 8);
    if (proto_crc16(p->buf + 1, 5 + len) == crc && cb)
        cb(p->buf[2], p->buf[3], p->buf + 6, len, ctx);
    return total;
}

void proto_parser_feed(proto_parser_t *p, const uint8_t *data, size_t len,
                       proto_frame_cb_t cb, void *ctx)
{
    while (len) {
        size_t room = sizeof(p->buf) - p->pos;
        size_t n = len < room ? len : room;
        memcpy(p->buf + p->pos, data, n);
        p->pos += n;
        data += n;
        len -= n;
        int consumed;
        while ((consumed = try_extract(p, cb, ctx)) > 0) {
            memmove(p->buf, p->buf + consumed, p->pos - consumed);
            p->pos -= consumed;
        }
        if (p->pos == sizeof(p->buf)) p->pos = 0; /* 无法成帧的满缓冲,丢弃 */
    }
}
