"""演示源:循环播放各状态/用量/context,用于联调硬件动画。"""

from __future__ import annotations

import asyncio
import itertools
import math
from collections.abc import AsyncIterator

from ..protocol import (AgentState, ContextCategory, ContextMsg, StateMsg,
                        TextMsg, UsageMsg, UsageSlot)
from .base import Source

SCRIPT = [
    (AgentState.IDLE, 3),
    (AgentState.THINKING, 4),
    (AgentState.TOOL_USE, 4),
    (AgentState.RESPONDING, 5),
    (AgentState.WAITING_USER, 3),
    (AgentState.ERROR, 2),
]


class DemoSource(Source):
    async def events(self) -> AsyncIterator[bytes]:
        t = 0.0
        for state, dur in itertools.cycle(SCRIPT):
            yield StateMsg(state).encode()
            yield TextMsg(1, f"[demo] state -> {state.name}\n").encode()
            end = t + dur
            while t < end:
                pct = int(50 + 49 * math.sin(t / 8))
                yield UsageMsg({
                    UsageSlot.SESSION: pct,
                    UsageSlot.LIMIT_5H: (pct * 2) % 100,
                }).encode()
                used = int(200_000 * (0.5 + 0.5 * math.sin(t / 13)))
                yield ContextMsg(used, 200_000, {
                    ContextCategory.SYSTEM: 20_000,
                    ContextCategory.TOOLS: 15_000,
                    ContextCategory.MESSAGES: max(0, used - 35_000),
                }).encode()
                await asyncio.sleep(0.5)
                t += 0.5
