"""Claude Code 状态源。

两路输入,二选一或并用:
1. JSONL tail:监视 ~/.claude/projects/**/*.jsonl 最新会话文件,从消息流推断
   状态(assistant 开始 → RESPONDING,tool_use → TOOL_USE,等待输入 → WAITING_USER)
   并从 usage 字段累计 context token。
2. Hooks(更实时,推荐补充):在 ~/.claude/settings.json 配置 hooks 向
   http://127.0.0.1:8765/event POST 事件,本源内置一个微型 HTTP listener 接收。
   示例 hook 配置见 host/README.md。
"""

from __future__ import annotations

import asyncio
import json
import logging
from collections.abc import AsyncIterator
from pathlib import Path

from ..protocol import (AgentState, ContextCategory, ContextMsg, StateMsg,
                        TextMsg, ToneMsg, UsageMsg, UsageSlot)
from .base import Source

log = logging.getLogger(__name__)

CLAUDE_PROJECTS = Path.home() / ".claude" / "projects"
CONTEXT_TOTAL = 200_000  # 默认 context window,可被 CONFIG 覆盖
IDLE_TIMEOUT = 30.0  # 无新事件多少秒后回落 IDLE


class ClaudeCodeSource(Source):
    def __init__(self, projects_dir: Path = CLAUDE_PROJECTS,
                 hook_port: int | None = 8765) -> None:
        self.projects_dir = projects_dir
        self.hook_port = hook_port
        self._queue: asyncio.Queue[bytes] = asyncio.Queue()
        self._state = AgentState.OFFLINE

    # ------------------------------------------------------------- helpers

    def _latest_transcript(self) -> Path | None:
        files = list(self.projects_dir.glob("*/*.jsonl"))
        return max(files, key=lambda p: p.stat().st_mtime, default=None)

    async def _set_state(self, state: AgentState, detail: int = 0) -> None:
        if state is not self._state:
            self._state = state
            await self._queue.put(StateMsg(state, detail).encode())
            if state is AgentState.WAITING_USER:
                await self._queue.put(ToneMsg(1).encode())  # attention 提示音

    async def _handle_record(self, rec: dict) -> None:
        rtype = rec.get("type")
        msg = rec.get("message") or {}
        if rtype == "user":
            await self._set_state(AgentState.THINKING)
            content = msg.get("content")
            if isinstance(content, str) and content.strip():
                await self._queue.put(TextMsg(0, content[:200], op=1).encode())
        elif rtype == "assistant":
            usage = msg.get("usage") or {}
            in_tok = (usage.get("input_tokens", 0)
                      + usage.get("cache_read_input_tokens", 0)
                      + usage.get("cache_creation_input_tokens", 0))
            if in_tok:
                await self._queue.put(ContextMsg(
                    used_tokens=in_tok, total_tokens=CONTEXT_TOTAL,
                    categories={ContextCategory.MESSAGES: in_tok},
                ).encode())
                await self._queue.put(UsageMsg({
                    UsageSlot.SESSION: min(100, in_tok * 100 // CONTEXT_TOTAL),
                }).encode())
            blocks = msg.get("content") or []
            kinds = {b.get("type") for b in blocks if isinstance(b, dict)}
            if "tool_use" in kinds:
                await self._set_state(AgentState.TOOL_USE)
            else:
                await self._set_state(AgentState.RESPONDING)
            for b in blocks:
                if isinstance(b, dict) and b.get("type") == "text" and b.get("text"):
                    await self._queue.put(TextMsg(1, b["text"][:200], op=0).encode())

    # ------------------------------------------------------------- tasks

    async def _tail_task(self) -> None:
        current: Path | None = None
        pos = 0
        last_event = asyncio.get_running_loop().time()
        while True:
            latest = self._latest_transcript()
            if latest != current:
                current, pos = latest, (latest.stat().st_size if latest else 0)
                if current:
                    log.info("tailing %s", current)
                    await self._set_state(AgentState.IDLE)
            if current and current.exists():
                size = current.stat().st_size
                if size > pos:
                    with current.open("r", encoding="utf-8", errors="replace") as f:
                        f.seek(pos)
                        for line in f:
                            try:
                                await self._handle_record(json.loads(line))
                                last_event = asyncio.get_running_loop().time()
                            except json.JSONDecodeError:
                                pass
                    pos = size
            now = asyncio.get_running_loop().time()
            if self._state in (AgentState.RESPONDING, AgentState.TOOL_USE,
                               AgentState.THINKING) and now - last_event > IDLE_TIMEOUT:
                await self._set_state(AgentState.WAITING_USER)
            await asyncio.sleep(0.5)

    async def _hook_server(self) -> None:
        """极简 HTTP 服务:POST /event {"hook_event_name": ...}。"""

        async def handle(reader: asyncio.StreamReader,
                         writer: asyncio.StreamWriter) -> None:
            try:
                raw = await asyncio.wait_for(reader.read(65536), timeout=2)
                body = raw.split(b"\r\n\r\n", 1)[-1]
                evt = json.loads(body) if body.strip() else {}
                name = evt.get("hook_event_name", "")
                mapping = {
                    "UserPromptSubmit": AgentState.THINKING,
                    "PreToolUse": AgentState.TOOL_USE,
                    "PostToolUse": AgentState.RESPONDING,
                    "Notification": AgentState.WAITING_USER,
                    "Stop": AgentState.WAITING_USER,
                    "SessionEnd": AgentState.IDLE,
                }
                if name in mapping:
                    await self._set_state(mapping[name])
                writer.write(b"HTTP/1.1 204 No Content\r\n\r\n")
                await writer.drain()
            except Exception:  # noqa: BLE001 — hook 输入不可信,吞掉即可
                pass
            finally:
                writer.close()

        server = await asyncio.start_server(handle, "127.0.0.1", self.hook_port)
        log.info("hook listener on 127.0.0.1:%d", self.hook_port)
        async with server:
            await server.serve_forever()

    async def events(self) -> AsyncIterator[bytes]:
        tasks = [asyncio.create_task(self._tail_task())]
        if self.hook_port:
            tasks.append(asyncio.create_task(self._hook_server()))
        try:
            while True:
                yield await self._queue.get()
        finally:
            for t in tasks:
                t.cancel()
