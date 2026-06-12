"""Agent Indicator 协议层:帧编码/解码 + 消息定义。

帧格式(WS / USB bulk / CDC 通用,CAN 走 transports.can 的分段映射):
  0xA9 | VER(1) | TYPE(1) | FLAGS(1) | LEN(2,LE) | PAYLOAD | CRC16(2,LE)
CRC16-CCITT (0x1021, init 0xFFFF),覆盖 VER..PAYLOAD。
"""

from __future__ import annotations

import enum
import struct
from dataclasses import dataclass, field

MAGIC = 0xA9
VERSION = 0x01
MAX_PAYLOAD = 512


class MsgType(enum.IntEnum):
    STATE = 0x01
    USAGE = 0x02
    CONTEXT = 0x03
    TEXT = 0x04
    TONE = 0x05
    CONFIG = 0x06
    # device -> host
    TELEMETRY = 0x80
    INPUT = 0x81
    MIC_LEVEL = 0x82


class AgentState(enum.IntEnum):
    IDLE = 0
    CONNECTING = 1
    THINKING = 2
    RESPONDING = 3
    TOOL_USE = 4
    WAITING_USER = 5
    ERROR = 6
    RATE_LIMITED = 7
    OFFLINE = 8


class UsageSlot(enum.IntEnum):
    SESSION = 0
    LIMIT_5H = 1
    LIMIT_WEEK = 2
    COST = 3


class ContextCategory(enum.IntEnum):
    SYSTEM = 0
    TOOLS = 1
    MCP = 2
    MEMORY = 3
    MESSAGES = 4
    FREE = 5


def crc16_ccitt(data: bytes, crc: int = 0xFFFF) -> int:
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


def encode_frame(msg_type: MsgType, payload: bytes, flags: int = 0) -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload too long: {len(payload)}")
    body = struct.pack("<BBBH", VERSION, msg_type, flags, len(payload)) + payload
    return bytes([MAGIC]) + body + struct.pack("<H", crc16_ccitt(body))


@dataclass
class Frame:
    msg_type: MsgType
    flags: int
    payload: bytes


class FrameParser:
    """增量式解析器,适配流式传输(CDC/USB bulk 粘包)。WS 一帧一消息也可直接喂。"""

    def __init__(self) -> None:
        self._buf = bytearray()

    def feed(self, data: bytes) -> list[Frame]:
        self._buf.extend(data)
        frames: list[Frame] = []
        while True:
            idx = self._buf.find(MAGIC)
            if idx < 0:
                self._buf.clear()
                break
            del self._buf[:idx]
            if len(self._buf) < 8:
                break
            ver, mtype, flags, length = struct.unpack_from("<BBBH", self._buf, 1)
            total = 1 + 5 + length + 2
            if ver != VERSION or length > MAX_PAYLOAD:
                del self._buf[:1]
                continue
            if len(self._buf) < total:
                break
            body = bytes(self._buf[1 : 6 + length])
            (crc,) = struct.unpack_from("<H", self._buf, 6 + length)
            del self._buf[:total]
            if crc16_ccitt(body) != crc:
                continue
            frames.append(Frame(MsgType(mtype), flags, body[5:]))
        return frames


# ---------------------------------------------------------------- messages

@dataclass
class StateMsg:
    state: AgentState
    detail: int = 0

    def encode(self) -> bytes:
        return encode_frame(MsgType.STATE, struct.pack("<BB", self.state, self.detail))


@dataclass
class UsageMsg:
    slots: dict[UsageSlot, int] = field(default_factory=dict)  # slot -> percent 0..100

    def encode(self) -> bytes:
        payload = bytes([len(self.slots)]) + b"".join(
            struct.pack("<BB", slot, min(100, max(0, pct))) for slot, pct in self.slots.items()
        )
        return encode_frame(MsgType.USAGE, payload)


@dataclass
class ContextMsg:
    used_tokens: int
    total_tokens: int
    categories: dict[ContextCategory, int] = field(default_factory=dict)  # cat -> tokens

    def encode(self) -> bytes:
        payload = struct.pack("<IIB", self.used_tokens, self.total_tokens, len(self.categories))
        for cat, tokens in self.categories.items():
            payload += struct.pack("<BI", cat, tokens)
        return encode_frame(MsgType.CONTEXT, payload)


@dataclass
class TextMsg:
    stream: int  # 0 = input, 1 = output
    text: str
    op: int = 0  # 0 append / 1 replace / 2 clear

    def encode(self) -> bytes:
        data = self.text.encode("utf-8")[: MAX_PAYLOAD - 2]
        return encode_frame(MsgType.TEXT, struct.pack("<BB", self.stream, self.op) + data)


@dataclass
class ToneMsg:
    tone_id: int  # 0 done / 1 attention / 2 error / 3 boot

    def encode(self) -> bytes:
        return encode_frame(MsgType.TONE, bytes([self.tone_id]))


@dataclass
class ConfigMsg:
    key: int  # 0 brightness / 1 matrix_tiles(1|4) / 2 orientation
    value: int

    def encode(self) -> bytes:
        return encode_frame(MsgType.CONFIG, struct.pack("<BI", self.key, self.value))


@dataclass
class Telemetry:
    cell_mv: tuple[int, int, int]
    ibat_ma: int
    soc: int
    chg_state: int
    power_src: int  # 0 bat / 1 pd / 2 xt30

    @classmethod
    def decode(cls, payload: bytes) -> "Telemetry":
        v1, v2, v3, i, soc, chg, src = struct.unpack("<HHHhBBB", payload[:11])
        return cls((v1, v2, v3), i, soc, chg, src)
