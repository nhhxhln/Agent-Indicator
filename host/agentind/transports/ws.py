"""Wi-Fi 链路:设备端起 WebSocket server 并广播 mDNS `_agentind._tcp`,
host(PC 同时作为 AP)经 zeroconf 发现后作为 client 连接。
"""

from __future__ import annotations

import asyncio
import logging

import websockets
from zeroconf import ServiceBrowser, ServiceStateChange, Zeroconf

from ..protocol import FrameParser
from .base import Transport

log = logging.getLogger(__name__)

SERVICE = "_agentind._tcp.local."


async def discover(timeout: float = 5.0) -> str | None:
    """返回首个发现的设备 ws://host:port/ws 地址,超时返回 None。"""
    loop = asyncio.get_running_loop()
    found: asyncio.Future[str] = loop.create_future()
    zc = Zeroconf()

    def on_change(zeroconf: Zeroconf, service_type: str, name: str,
                  state_change: ServiceStateChange) -> None:
        if state_change is not ServiceStateChange.Added or found.done():
            return
        info = zeroconf.get_service_info(service_type, name)
        if info and info.addresses:
            addr = ".".join(str(b) for b in info.addresses[0])
            loop.call_soon_threadsafe(
                found.set_result, f"ws://{addr}:{info.port}/ws")

    browser = ServiceBrowser(zc, SERVICE, handlers=[on_change])
    try:
        return await asyncio.wait_for(found, timeout)
    except asyncio.TimeoutError:
        return None
    finally:
        browser.cancel()
        zc.close()


class WsTransport(Transport):
    def __init__(self, url: str | None = None) -> None:
        super().__init__()
        self.url = url
        self._ws: websockets.WebSocketClientProtocol | None = None
        self._rx_task: asyncio.Task | None = None
        self._parser = FrameParser()

    @property
    def name(self) -> str:
        return f"ws({self.url})"

    async def open(self) -> None:
        if self.url is None:
            self.url = await discover()
            if self.url is None:
                raise ConnectionError("mDNS 未发现设备,请确认 ESP32 已接入 AP")
        self._ws = await websockets.connect(self.url, max_size=4096)
        self._rx_task = asyncio.create_task(self._rx_loop())
        log.info("connected to %s", self.url)

    async def _rx_loop(self) -> None:
        assert self._ws
        try:
            async for msg in self._ws:
                if isinstance(msg, bytes) and self.on_rx:
                    for frame in self._parser.feed(msg):
                        self.on_rx(frame)
        except websockets.ConnectionClosed:
            log.warning("ws closed")

    async def send(self, frame: bytes) -> None:
        if self._ws is None:
            raise ConnectionError("not connected")
        await self._ws.send(frame)

    async def close(self) -> None:
        if self._rx_task:
            self._rx_task.cancel()
        if self._ws:
            await self._ws.close()
