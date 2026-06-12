"""传输层抽象:三条链路(Wi-Fi WS / SocketCAN / USB vendor EP)统一接口。"""

from __future__ import annotations

import abc
from collections.abc import Callable

from ..protocol import Frame

RxCallback = Callable[[Frame], None]


class Transport(abc.ABC):
    """所有传输实现的基类。send() 接收完整协议帧字节;收到设备帧回调 on_rx。"""

    def __init__(self) -> None:
        self.on_rx: RxCallback | None = None

    @abc.abstractmethod
    async def open(self) -> None: ...

    @abc.abstractmethod
    async def close(self) -> None: ...

    @abc.abstractmethod
    async def send(self, frame: bytes) -> None: ...

    @property
    @abc.abstractmethod
    def name(self) -> str: ...
