"""CAN 链路:SocketCAN(PC 侧 USB-CAN 适配器,`ip link set can0 up type can bitrate 500000`)。

映射规则(与固件 comm_twai.c 对应):
- 11-bit ID = 0x500 | (TYPE & 0x1F);设备→host 用 0x580 | TYPE 低 5 位。
- 协议帧的 *payload 部分*(不含 MAGIC/CRC,CAN 自带校验)分段传输:
  byte0 = (seq << 4) | total_segments,首段 byte1 = TYPE 原值,其余字节为数据。
"""

from __future__ import annotations

import asyncio
import logging
import struct

from ..protocol import Frame, MsgType
from .base import Transport

log = logging.getLogger(__name__)

CAN_ID_H2D_BASE = 0x500
CAN_ID_D2H_BASE = 0x580


class CanTransport(Transport):
    def __init__(self, channel: str = "can0", bitrate: int = 500000) -> None:
        super().__init__()
        self.channel = channel
        self.bitrate = bitrate
        self._bus = None
        self._notifier = None
        self._reasm: dict[int, list[bytes | None]] = {}

    @property
    def name(self) -> str:
        return f"can({self.channel})"

    async def open(self) -> None:
        import can  # python-can,可选依赖

        self._bus = can.Bus(interface="socketcan", channel=self.channel)
        loop = asyncio.get_running_loop()
        self._notifier = can.Notifier(self._bus, [self._on_msg], loop=loop)
        log.info("socketcan %s up", self.channel)

    def _on_msg(self, msg) -> None:
        if not (CAN_ID_D2H_BASE <= msg.arbitration_id < CAN_ID_D2H_BASE + 0x20):
            return
        data = bytes(msg.data)
        if len(data) < 2:
            return
        seq, total = data[0] >> 4, data[0] & 0x0F
        key = msg.arbitration_id
        if seq == 0:
            self._reasm[key] = [None] * total
        segs = self._reasm.get(key)
        if segs is None or seq >= len(segs):
            return
        segs[seq] = data[1:]
        if all(s is not None for s in segs):
            raw = b"".join(segs)  # type: ignore[arg-type]
            del self._reasm[key]
            mtype, payload = raw[0], raw[1:]
            if self.on_rx:
                try:
                    self.on_rx(Frame(MsgType(mtype), 0, payload))
                except ValueError:
                    log.warning("unknown msg type 0x%02x", mtype)

    async def send(self, frame: bytes) -> None:
        import can

        assert self._bus is not None
        # 拆开通用帧取 TYPE 与 payload(can 链路不传 MAGIC/CRC)
        mtype = frame[2]
        (length,) = struct.unpack_from("<H", frame, 4)
        payload = frame[6 : 6 + length]
        raw = bytes([mtype]) + payload
        total = max(1, (len(raw) + 6) // 7)
        if total > 15:
            raise ValueError("payload too long for CAN segmentation")
        for seq in range(total):
            chunk = raw[seq * 7 : seq * 7 + 7]
            data = bytes([(seq << 4) | total]) + chunk
            self._bus.send(can.Message(
                arbitration_id=CAN_ID_H2D_BASE | (mtype & 0x1F),
                data=data, is_extended_id=False))
            await asyncio.sleep(0)  # 让出事件循环

    async def close(self) -> None:
        if self._notifier:
            self._notifier.stop()
        if self._bus:
            self._bus.shutdown()
