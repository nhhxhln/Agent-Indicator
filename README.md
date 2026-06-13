# Agent Indicator

中文 | [English](README.en.md)

> 一个为 LLM Agent 打造的状态指示桌搭:让 Claude Code 的思考、回复、工具调用、
> 用量与 context 占用,变成桌面上一眼可读的光与屏。

![Console 主力款](docs/images/console-3d.png)

## ✨ 它能做什么

| 信息 | 载体 | 效果 |
|---|---|---|
| Agent 状态 | RGB Circle ×24 | 思考=紫色彗星旋转 · 回复=绿色流光 · 工具=双彗星对转 · 等待=蓝色呼吸 |
| Context 占用 | RGB Matrix 8×8(可 4 块拼 16×16) | 按 system/tools/mcp/memory/messages 分类的热力图 |
| 用量额度 | RGB Bar 20LED | 会话/5h 限额双段渐变条 |
| 输入输出 | 480×480 圆形视觉 LCD + 触摸 | 文本流 + 六页触摸 UI |
| 拾音电平 | RGB Bar(最长 160LED@329mm) | 中心展开 VU,MIC 本地闭环 |
| 提示音 | ES8311 + 3W PA | 完成/注意/错误三种音色 |
| 灯效模式 | 全部灯带 | OpenRGB 风格:常亮(调色)/呼吸/跑马灯/彩虹/关闭 |
| 环境传感 | SHT4x / BMP280 / PCF8563 | 温湿度 / 气压 / RTC 时钟,Devices 页实时显示 |
| 界面主题 | LCD | Light / Dark 双主题,Devices 页一键切换,NVS 持久化 |

**三链路接入**:Wi-Fi(WebSocket + mDNS 自发现)/ CAN(SocketCAN)/ USB 自定义端点,
同时在线自动择路。**电源**:USB-PD 15V / XT30 12-24V / 3×18650 平衡充,NVDC 无缝切换。

## 🖼️ 外观方案(6 款,共用同一主板)

| A「Halo」圆形单体 | D「Tiles」磁吸模块 | F「Orb」球形桌宠 |
|:---:|:---:|:---:|
| ![Halo](docs/images/halo-3d.png) | ![Tiles](docs/images/tiles-3d.png) | ![Orb](docs/images/orb-3d.png) |

| B「Console」(推荐主力款) | E「Soundbar」(显示器下横条) |
|:---:|:---:|
| ![Console](docs/images/console-3d.png) | ![Soundbar](docs/images/soundbar-3d.png) |

> 另有 C「Totem」竖塔款,详见 [外观设计文档](docs/03-industrial-design.md)。

## 📱 屏幕 UI(六页 + Light/Dark 主题,PC 模拟器渲染 = 固件像素一致)

| Home 状态+IO 流 | Lighting 灯效 | Wi-Fi 连接 |
|:---:|:---:|:---:|
| ![Home](docs/images/ui/ui-home.png) | ![Lighting](docs/images/ui/ui-light.png) | ![Wi-Fi](docs/images/ui/ui-wifi.png) |

| Devices 检测+传感器 | Files 浏览 | Music 播放 |
|:---:|:---:|:---:|
| ![Devices](docs/images/ui/ui-devices.png) | ![Files](docs/images/ui/ui-files.png) | ![Music](docs/images/ui/ui-music.png) |

| Dark 主题(默认) | Light 主题 |
|:---:|:---:|
| ![Home dark](docs/images/ui/ui-home.png) | ![Home light](docs/images/ui/ui-light-home.png) |

## 🚀 快速开始

```bash
# 1. 固件(ESP-IDF v5.2)
cd firmware && idf.py set-target esp32s3 && idf.py menuconfig   # 填 Wi-Fi SSID
idf.py build flash monitor

# 2. 上位机(PC 先建好 AP)
cd host && pip install -e . && agentind run        # Claude Code 状态自动转发
agentind run -s demo -v                             # 或先跑演示动画联调

# 3. UI 模拟器(可选,PC 上改 UI 不烧板)
cd tools/ui_sim && cmake -B build && cmake --build build -j && ./build/ui_sim shots/
```

## 📚 文档导航

| 文档 | 内容 |
|---|---|
| [01 系统设计](docs/01-system-design.md) | 架构、器件选型、模块详设、三链路协议 |
| [02 电源设计](docs/02-power-design.md) | PD/XT30/3S 拓扑、充电均衡保护、功耗预算 |
| [03 外观设计](docs/03-industrial-design.md) | 6 方案 + 3D 渲染 + 对比选型 |
| [04 原理图划分](docs/04-schematic-partition.md) | 8 页划分、完整 pinmap、LCD 40P 定义、布局 |
| [05 屏幕 UI](docs/05-ui-design.md) | 六页 UI 设计 + 模拟器用法 |
| [host/README](host/README.md) | 上位机安装、Claude Code hooks 接入 |
| [firmware/README](firmware/README.md) | 编译烧录、测试控制台命令表、任务栈约定 |

## 📂 仓库结构

```
docs/            设计文档(中文)· docs/en/ 英文版 · docs/images/ 渲染图与 UI 截图
hardware/3d/     OpenSCAD 模型源文件 + 可打印 STL(6 款外观)
host/            Python 桥 agentind:状态源 → 协议 → ws/can/usb
firmware/        ESP-IDF v5.2:三链路 comm / LED 引擎+灯效 / LVGL9 六页 UI /
                 音频五项 case / CAN case / IMU 可视化 / 测试控制台(UART)
tools/ui_sim/    LVGL PC 无头模拟器(与固件共用 UI 源码,出文档截图)
```

## 🗺️ 路线图

- [x] 设计文档 / 电源方案 / 协议 / 固件框架 / 上位机
- [x] LVGL 六页 UI + 灯效引擎 + PC 模拟器
- [ ] 原理图绘制(KiCad)与主板打样
- [ ] ST7701 屏厂初始化序列核对、USB vendor 描述符
- [ ] Console(方案 B)结构件打印与整机联调
- [ ] Tiles 模块化(pogo 枚举协议)二期
