from .base import Transport
from .ws import WsTransport


def create(kind: str, **kwargs) -> Transport:
    """按名称构造传输:ws / can / usb。can 与 usb 依赖可选包,延迟导入。"""
    if kind == "ws":
        return WsTransport(**kwargs)
    if kind == "can":
        from .can import CanTransport
        return CanTransport(**kwargs)
    if kind == "usb":
        from .usb import UsbTransport
        return UsbTransport(**kwargs)
    raise ValueError(f"unknown transport: {kind}")


__all__ = ["Transport", "WsTransport", "create"]
