from .base import Source
from .claude_code import ClaudeCodeSource
from .demo import DemoSource


def create(kind: str, **kwargs) -> Source:
    if kind in ("claude", "claude-code"):
        return ClaudeCodeSource(**kwargs)
    if kind == "demo":
        return DemoSource(**kwargs)
    raise ValueError(f"unknown source: {kind}")


__all__ = ["Source", "ClaudeCodeSource", "DemoSource", "create"]
