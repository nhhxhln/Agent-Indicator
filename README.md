# Agent Indicator

中文 | [English](README.en.md)

LLM 状态指示桌搭:RGB Matrix(context)· RGB Circle(status)· RGB Bar(usage / 拾音 VU)·
LCD(输入输出),ESP32-S3 主控,三链路接入(Wi-Fi / CAN / USB),3×18650 + PD/XT30 供电。

![Console](docs/images/console-3d.png)

```
docs/
  01-system-design.md        系统架构、器件选型、模块详设、通讯协议
  02-power-design.md         电源拓扑、充电/保护/均衡、功耗预算
  03-industrial-design.md    外观 4 方案(Halo / Console / Totem / Tiles)+ 3D 渲染
  04-schematic-partition.md  原理图 8 页划分、pinmap、布局分区
  en/                        English versions
hardware/3d/                 OpenSCAD 模型源文件 + 可打印 STL
host/                        Python 转发软件 agentind(状态源 → 协议 → ws/can/usb)
firmware/                    ESP-IDF v5.2 固件(LVGL9 / 音频 / CAN / IMU / 测试控制台)
```

快速开始:见 `host/README.md` 与 `firmware/README.md`。
硬件路线建议:先做 Console(方案 B)一体款,主板四方案通用。
