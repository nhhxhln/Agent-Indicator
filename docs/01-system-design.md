# Agent Indicator — 系统设计总览

> LLM 状态指示桌搭 · Rev 0.2 · 2026-06
> English version: [en/01-system-design.md](en/01-system-design.md)
> 配套文档:[02-power-design.md](02-power-design.md) 电源 · [03-industrial-design.md](03-industrial-design.md) 外观 · [04-schematic-partition.md](04-schematic-partition.md) 原理图

## 1. 功能定义

| 功能 | 载体 | 说明 |
|---|---|---|
| LLM Usage 展示 | RGB Bar(20 LED 竖条/分段) | 会话用量、5h/周限额等多槽位百分比条 |
| LLM Status 展示 | RGB Circle(24 LED) | idle / thinking / responding / tool-use / error 等状态动画 |
| LLM Context 展示 | RGB Matrix(8×8,固件兼容 4 块拼 16×16) | context window 占用热力图 / 分类色块 |
| LLM 输入输出 | LCD(ST7701 480×480 + CST820 触摸) | 滚动显示 prompt/response 摘要,触摸翻页 |
| 拾音电平 | RGB Bar(横向长条) | 本地 MIC VU 表 / 提示音可视化 |
| 提示音 / 拾音 | I2S Codec + PA + MEMS MIC | 任务完成提示音、语音输入电平 |

扩展接口:SDMMC(资源/录音)、CAN/TWAI(总线接入)、USB Device(自定义 EP + CDC)、I2C 外扩(触摸 + 传感器)。

## 2. 系统框图

```
                        ┌────────────────────────────────────────────┐
   Linux PC (Host)      │              Agent Indicator               │
  ┌──────────────┐      │  ┌──────────────────────────────────────┐  │
  │ agentind     │ WiFi │  │           ESP32-S3-WROOM-1-N16R8     │  │
  │ (Python 转发)│◄────►│  │                                      │  │
  │  - 状态源    │ CAN  │  │ RGB-LCD16bit ──► ST7701 480×480 LCD  │  │
  │  - 协议编码  │◄────►│  │ I2C ──► CST820 / QMI8658C / TCA9554  │  │
  │  - 多路传输  │ USB  │  │            / INA226 / MP2760         │  │
  └──────────────┘◄────►│  │ RMT×2 ──► WS2812B Matrix / Ring+Bars │  │
                        │  │ I2S ──► ES8311 ──► NS4150B ──► SPK   │  │
                        │  │              ▲MIC                    │  │
                        │  │ SDMMC 1-bit ──► microSD              │  │
                        │  │ TWAI ──► TJA1051T/3 ──► CAN(XT30 旁) │  │
                        │  │ USB-OTG ──► Type-C(数据,与 PD 口分开)│  │
                        │  └──────────────────────────────────────┘  │
                        │  电源:PD(15V)/XT30(12-24V)/3S 18650      │
                        └────────────────────────────────────────────┘
```

## 3. 器件选型

### 3.1 主控

| 项 | 选型 | 理由 |
|---|---|---|
| MCU 模组 | **ESP32-S3-WROOM-1-N16R8** | 16MB Flash + 8MB Octal PSRAM。RGB LCD 480×480×RGB565 帧缓冲 ≈460KB 必须放 PSRAM;LCD_CAM 外设直驱 RGB 屏;原生 USB-OTG;TWAI;RMT 驱 WS2812;Wi-Fi |

注意:N16R8 的 Octal PSRAM 占用 GPIO35/36/37,引脚预算见 04 文档 pinmap。

### 3.2 外围芯片

| 功能 | 选型 | 备选 | 说明 |
|---|---|---|---|
| CAN 收发器 | **TJA1051T/3** | SIT1051T(国产) | /3 版本 VIO 3.3V,无需电平转换;5V 供电自 5V0 |
| Audio Codec | **ES8311** | ES8388(立体声) | 单声道 ADC+DAC,I2S + I2C 控制,ESP-IDF `esp_codec_dev` 原生支持;支持 BCLK 作 MCLK 源(省 1 个 GPIO) |
| Audio PA | **NS4150B** | MAX98357(纯 I2S 方案) | 3W Class-D,Espressif 官方板同款搭配;EN 脚接 IO 扩展器 |
| MIC | **MSM261(模拟 MEMS)→ ES8311 MIC_IN** | INMP441(I2S 数字,需多占线) | 走 codec 模拟输入,省 I2S 线;拾音条 VU 由固件 RMS 计算 |
| IMU | **QMI8658C** | LSM6DS3TR-C | 6 轴,I2C,带敲击/抬手中断,资料多价格低;用于敲击交互、姿态感知翻转显示 |
| 触摸 | **CST820**(随屏) | — | I2C 0x15,与 CST816 同协议族,可用 `esp_lcd_touch_cst816s` 驱动 |
| IO 扩展 | **TCA9554** | PCA9554 | LCD_RST / TP_RST / PA_EN / BL_EN / LED_PWR_EN 等慢速控制,缓解 GPIO 紧张 |
| 电流监控 | **INA226** | INA219 | 挂 VSYS 高侧,电量/功耗遥测上报 |
| LED 电平转换 | **SN74AHCT125** | 74HCT245 | 3.3V→5V 数据,WS2812B 在 5V 供电下 VIH=3.5V,必须转换 |

电源链路器件(CH224K / MP2760 / S-8254A / HY2213 / TPS56637 / MP2315S / TPS2121 / SGM2212)见 [02-power-design.md](02-power-design.md)。

### 3.3 显示与灯

| 项 | 参数 |
|---|---|
| LCD | ST7701S,480×480,16-bit RGB565 并口 + 3-wire SPI 初始化(SPI 线借用 RGB 数据线,Espressif 参考设计同款做法);外形 73.03×76.48mm,AA 70.13×70.13mm |
| RGB Matrix | WS2812B 8×8,64×64mm,板对板/排线级联,固件支持 1/4 块(16×16)拼接,S 形扫描可配置 |
| RGB Circle | WS2812B ×24,ID 75 / OD 85mm |
| RGB Bar | 按外形方案选长度:Usage 竖条 20LED@41.5mm;拾音条 48LED@99mm 或 64LED@132mm(见 03 文档各方案) |

## 4. 模块详设

### 4.1 LED 子系统

- **驱动**:RMT ×2 通道(`led_strip` 组件,DMA 模式)。
  - CH0 `LED_MATRIX`:matrix 链(1~4 块菊花链,64~256 颗)。
  - CH1 `LED_AUX`:circle(24)→ usage bar(20)→ 拾音条(48/64),一条链逻辑分段寻址。
- **刷新**:60fps 动画引擎,链长 172 颗 @800kHz ≈5.2ms,余量充足。
- **限流**:固件全局电流限制器——按帧累加理论电流(每通道 16mA 折算),超 VLED 预算(4.5A)整帧等比降亮。16×16 拼接时尤其必要。
- **供电**:VLED 5V/5A,LED_PWR_EN(TCA9554)控制 PMOS 开关,休眠彻底断电。

### 4.2 LCD 子系统

- `esp_lcd` RGB panel + `esp_lcd_st7701`(managed component),双 bounce buffer 抗 PSRAM 带宽抖动;PCLK 16MHz 起步。
- LVGL 9(`esp_lvgl_port`):I/O 文本流页面、状态页、设置页;CST820 触摸滑动切页。
- 背光:BL_EN(TCA9554)开关 + 屏内容自适应调光;若需硬件 PWM 调光,牺牲 TWAI_TX 引脚换(pinmap 中注明)。

### 4.3 音频子系统

- ES8311:I2S 从机,BCLK 作内部 MCLK(`MCLK from SCLK` 模式),16kHz/16bit 拾音,44.1kHz 提示音播放。
- 提示音:SD 卡或 flash 内嵌 WAV;`esp_codec_dev` 统一读写。
- 拾音条:ADC 流 20ms 窗口 RMS → dB 映射 → 拾音 bar 渐变,本地闭环不经上位机。

### 4.4 通讯子系统(三链路并存,统一协议层)

| 链路 | 实现 | 场景 |
|---|---|---|
| Wi-Fi | 设备起 WebSocket server(`esp_http_server`)+ mDNS `_agentind._tcp`;PC 做 AP,host 经 zeroconf 发现后连接 | 主链路,桌面摆放无线 |
| CAN | TWAI 500kbps,11-bit ID,>8B 消息用轻量分段(见 §5.3);PC 侧 USB-CAN + SocketCAN | 工控/多设备总线 |
| USB | TinyUSB 复合设备:Vendor class bulk EP(协议帧)+ CDC(日志/调试) | 直连零配置、固件调试 |

三条链路同时监听,最近收到有效帧的链路为"活动链路";遥测向活动链路回报。

### 4.5 电源管理

- MP2760(I2C)+ INA226 + 3S 电压采样 → 电量估算、充电状态;低电量时 circle 边缘呼吸红色提醒。
- 无活动 5min:LCD 降亮、LED 切低亮时钟/呼吸待机;30min:断 VLED、LCD 休眠,触摸/IMU 敲击唤醒。

## 5. 通讯协议(host ↔ device)

### 5.1 帧格式(WS / USB / CDC 通用)

```
┌──────┬─────┬──────┬──────┬─────────┬─────────┬───────┐
│ 0xA9 │ VER │ TYPE │ FLAGS│ LEN(LE) │ PAYLOAD │ CRC16 │
│  1B  │ 1B  │  1B  │  1B  │   2B    │  ≤512B  │  2B   │
└──────┴─────┴──────┴──────┴─────────┴─────────┴───────┘
```
- VER=0x01;CRC16-CCITT(覆盖 VER..PAYLOAD);WS binary message 一帧一消息。

### 5.2 消息类型

| TYPE | 方向 | 名称 | Payload |
|---|---|---|---|
| 0x01 | H→D | STATE | `u8 state, u8 detail`。state: 0 IDLE / 1 CONNECTING / 2 THINKING / 3 RESPONDING / 4 TOOL_USE / 5 WAITING_USER / 6 ERROR / 7 RATE_LIMITED / 8 OFFLINE |
| 0x02 | H→D | USAGE | `u8 n, n×{u8 slot, u8 percent}`。slot: 0 会话 / 1 5h 限额 / 2 周限额 / 3 成本 |
| 0x03 | H→D | CONTEXT | `u32 used, u32 total, u8 n, n×{u8 category, u32 tokens}`。category: system/tools/mcp/memory/messages/free |
| 0x04 | H→D | TEXT | `u8 stream(0 in/1 out), u8 op(0 append/1 replace/2 clear), utf8[]` |
| 0x05 | H→D | TONE | `u8 tone_id`(0 done / 1 attention / 2 error / 3 boot) |
| 0x06 | H→D | CONFIG | `u8 key, u32 value`。key: 0 亮度 / 1 matrix 块数(1\|4)/ 2 方向 / 3 UI 语言(0 en, 1 zh) |
| 0x80 | D→H | TELEMETRY | `u16 vbat_mV ×3cell, i16 ibat_mA, u8 soc, u8 chg_state, u8 src(0 bat/1 pd/2 xt30)` |
| 0x81 | D→H | INPUT | `u8 kind(0 touch/1 imu_tap/2 imu_shake), u8 arg` |
| 0x82 | D→H | MIC_LEVEL | `u8 db`(可选订阅) |

### 5.3 CAN 映射

11-bit ID = `0x500 | TYPE 低 5 位`;Payload >7B 时分段:`byte0 = (seq<<4)|total`,首段 byte1 为 TYPE 原值。TELEMETRY 单帧直发 `0x580`。

## 6. 软件交付物

```
host/      Python 包 agentind:状态源(Claude Code hooks/JSONL)→ 协议编码 → ws/can/usb 传输
firmware/  ESP-IDF v5.2 工程:comm(三链路)/ proto / ui(matrix·ring·bar·lcd)/ audio / power
           + storage(SPIFFS/SD)/ console(测试 REPL)/ cases(CAN·Audio·IMU 测试)
```

详见各目录 README。

## 7. 固件平台能力(v0.2 新增)

| 能力 | 实现 | 说明 |
|---|---|---|
| 文件系统 | SPIFFS(6MB `storage` 分区 → `/spiffs`)+ microSD(1-bit SDMMC → `/sdcard`,支持热插拔挂载) | LVGL 经 `LV_USE_FS_POSIX` 以盘符 `A:` 直接访问两者 |
| 矢量字体 | LVGL TinyTTF(基于 stb_truetype,**非 FreeType**) | 开机检测 `/spiffs/fonts/ui.ttf` 自动加载;支持 .ttf 与 TrueType 轮廓的 .otf,**不支持 CFF/PostScript 轮廓的 .otf**,无 hinting/kerning。需要完整 OTF 时可换 `lv_freetype` + FreeType(约 +600KB) |
| UI 多语言 | `ui/i18n.c` 中英字符串表,NVS 持久化 | 控制台 `lang zh\|en` 或协议 CONFIG key=3 切换;中文渲染需 CJK TTF(内置 Montserrat 无中文字形) |
| LVGL | LVGL9 + esp_lvgl_port,双 FB direct mode + bounce buffer 抗撕裂 | LVGL 任务栈 10KB(TinyTTF 栅格化在该任务执行) |
| 测试控制台 | esp_console REPL @ USB Serial/JTAG | 与 TinyUSB vendor EP 互斥(同一 USB PHY),量产固件二选一 |
| 测试 case | CAN tx/rx、Audio rec/replay/player_from_sd/rec_to_sd/loopback、IMU 可视化 | C++ 实现(`cases/`),命令清单见 firmware/README;无屏/无外设时自动降级日志模式 |
| C++ 支持 | 新模块以 C++17 编写,经 `extern "C"` 与 C 框架互操作 | 任务栈显式标定:LVGL 10K / REPL 8K / player 8K / capture 6K / imu_vis 6K / can 4K |

### 7.1 音频数据流设计

单一 capture 任务独占 I2S RX:常态计算 VU(拾音条),录音(RAM/SD)与
loopback 复用同一数据流按模式分发;TX 由互斥锁串行化(tone/player/loopback
不并发),避免多任务争抢 I2S 句柄。
