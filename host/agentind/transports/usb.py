"""USB 链路:TinyUSB Vendor class,自定义 bulk EP(OUT 0x01 / IN 0x81)。

udev 规则(Linux,免 root):
  SUBSYSTEM=="usb", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="82a9", MODE="0666"
"""

from __future__ import annotations

import asyncio
import logging

from ..protocol import FrameParser
from .base import Transport

log = logging.getLogger(__name__)

VID = 0x303A  # Espressif
PID = 0x82A9  # 自定义,与固件 usb_descriptors 一致
EP_OUT = 0x01
EP_IN = 0x81


class UsbTransport(Transport):
    def __init__(self) -> None:
        super().__init__()
        self._dev = None
        self._rx_task: asyncio.Task | None = None
        self._parser = FrameParser()

    @property
    def name(self) -> str:
        return "usb(vendor-ep)"

    async def open(self) -> None:
        import usb.core  # pyusb,可选依赖

        self._dev = usb.core.find(idVendor=VID, idProduct=PID)
        if self._dev is None:
            raise ConnectionError("USB 设备未找到(VID:PID 303a:82a9)")
        # vendor interface 由固件放在 interface 0;CDC 在其后
        if self._dev.is_kernel_driver_active(0):
            self._dev.detach_kernel_driver(0)
        self._dev.set_configuration()
        self._rx_task = asyncio.create_task(self._rx_loop())
        log.info("usb vendor interface opened")

    async def _rx_loop(self) -> None:
        import usb.core

        loop = asyncio.get_running_loop()
        while True:
            try:
                data = await loop.run_in_executor(
                    None, lambda: self._dev.read(EP_IN, 512, timeout=200))
            except usb.core.USBTimeoutError:
                continue
            except usb.core.USBError as e:
                log.warning("usb read error: %s", e)
                return
            if self.on_rx:
                for frame in self._parser.feed(bytes(data)):
                    self.on_rx(frame)

    async def send(self, frame: bytes) -> None:
        assert self._dev is not None
        loop = asyncio.get_running_loop()
        await loop.run_in_executor(None, lambda: self._dev.write(EP_OUT, frame))

    async def close(self) -> None:
        if self._rx_task:
            self._rx_task.cancel()
        self._dev = None
