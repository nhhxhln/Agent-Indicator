"""状态源抽象:产出协议消息流,由 bridge 推给传输层。"""

from __future__ import annotations

import abc
from collections.abc import AsyncIterator


class Source(abc.ABC):
    """实现 events() 异步生成器,yield 已 encode 的协议帧(bytes)。"""

    @abc.abstractmethod
    def events(self) -> AsyncIterator[bytes]: ...
