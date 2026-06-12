# agentind — 上位机转发软件

中文 | [English](README.en.md)

读取 LLM(Claude Code)状态,按 [协议](../docs/01-system-design.md#5-通讯协议host--device) 编码后经
Wi-Fi WebSocket / SocketCAN / USB vendor EP 推送到 Agent Indicator。

## 安装

```bash
cd host
pip install -e .            # 基础(ws)
pip install -e ".[all]"     # + can/usb 支持
```

## 运行

```bash
agentind run                          # claude-code 源 + mDNS 发现设备(Wi-Fi)
agentind run -s demo -v               # 演示动画,联调硬件用
agentind run -t can --channel can0    # SocketCAN(先 ip link set can0 up type can bitrate 500000)
agentind run -t usb                   # USB 自定义端点(303a:82a9)
```

## Claude Code 状态接入

默认 tail `~/.claude/projects/**/*.jsonl` 推断状态;如需更实时,在
`~/.claude/settings.json` 增加 hooks,事件会推到内置 listener(127.0.0.1:8765):

```json
{
  "hooks": {
    "UserPromptSubmit": [{"hooks": [{"type": "command",
      "command": "curl -s -m1 -X POST -d @- http://127.0.0.1:8765/event"}]}],
    "Stop": [{"hooks": [{"type": "command",
      "command": "curl -s -m1 -X POST -d @- http://127.0.0.1:8765/event"}]}],
    "PreToolUse": [{"hooks": [{"type": "command",
      "command": "curl -s -m1 -X POST -d @- http://127.0.0.1:8765/event"}]}],
    "Notification": [{"hooks": [{"type": "command",
      "command": "curl -s -m1 -X POST -d @- http://127.0.0.1:8765/event"}]}]
  }
}
```

## 结构

```
agentind/
  protocol.py        帧编码/解码、消息定义(与固件 main/proto 对应)
  transports/        ws(mDNS 发现) / can(分段重组) / usb(bulk EP)
  sources/           claude_code(JSONL tail + hooks listener) / demo
  cli.py             agentind run
```
