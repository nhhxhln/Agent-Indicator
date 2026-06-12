# agentind — Host Bridge

[中文](README.md) | English

Reads LLM (Claude Code) state, encodes it per the
[protocol](../docs/en/01-system-design.md#5-protocol-host--device) and pushes it to the
Agent Indicator over Wi-Fi WebSocket / SocketCAN / USB vendor EP.

## Install

```bash
cd host
pip install -e .            # base (ws)
pip install -e ".[all]"     # + can/usb
```

## Run

```bash
agentind run                          # claude-code source + mDNS discovery (Wi-Fi)
agentind run -s demo -v               # demo animations for hardware bring-up
agentind run -t can --channel can0    # SocketCAN (ip link set can0 up type can bitrate 500000)
agentind run -t usb                   # USB custom endpoints (303a:82a9)
```

## Claude Code integration

By default it tails `~/.claude/projects/**/*.jsonl` to infer state. For lower latency
add hooks in `~/.claude/settings.json` that POST events to the built-in listener
(127.0.0.1:8765) — see the Chinese README for the JSON snippet (identical config).

## Layout

```
agentind/
  protocol.py        frame encode/decode, messages (mirrors firmware main/proto)
  transports/        ws (mDNS discovery) / can (segmentation) / usb (bulk EP)
  sources/           claude_code (JSONL tail + hooks listener) / demo
  cli.py             agentind run
```
