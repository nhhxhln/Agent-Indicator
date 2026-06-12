# Agent Indicator

LLM 状态指示桌搭:RGB Matrix(context)· RGB Circle(status)· RGB Bar(usage / 拾音 VU)·
LCD(输入输出),ESP32-S3 主控,三链路接入(Wi-Fi / CAN / USB),3×18650 + PD/XT30 供电。

```
docs/
  01-system-design.md        系统架构、器件选型、模块详设、通讯协议
  02-power-design.md         电源拓扑、充电/保护/均衡、功耗预算
  03-industrial-design.md    外观 4 方案(Halo / Console / Totem / Tiles)
  04-schematic-partition.md  原理图 8 页划分、pinmap、布局分区
host/                        Python 转发软件 agentind(状态源 → 协议 → ws/can/usb)
firmware/                    ESP-IDF v5.2 固件
```

快速开始:见 `host/README.md` 与 `firmware/README.md`。
硬件路线建议:先做 Console(方案 B)一体款,主板四方案通用。
