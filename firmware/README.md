# Agent Indicator 固件(ESP-IDF v5.2)

中文 | [English](README.en.md)

## 编译烧录

```bash
. /home/zhe/Code/esp-idf/export.sh
idf.py set-target esp32s3
idf.py menuconfig        # Agent Indicator 菜单:Wi-Fi SSID/密码、matrix 块数、VLED 限流
idf.py build flash monitor
```

## 结构

```
main/
  board.h            pinmap(与 docs/04 §2 对应,改板只动这里)
  app_main.c         启动顺序
  app_state.{h,c}    全局状态模型:comm 写入、ui/audio 消费;文本环形缓冲
  proto/             协议帧解析/构造(与 host/agentind/protocol.py 镜像)
  bus/i2c_bus.cpp    共享 I2C 总线 + TCA9554 扩展器(不在位时自动降级 no-op)
  storage/           SPIFFS(/spiffs)+ microSD 1-bit(/sdcard)
  console/           esp_console REPL @ USB Serial/JTAG,case 注册中枢
  comm/
    comm_wifi.c      STA + WebSocket server(/ws)+ mDNS _agentind._tcp;活动链路调度
    comm_twai.c      500kbps,0x500/0x580 段协议
    comm_usb.c       TinyUSB vendor EP 骨架(描述符待回板补全;与 REPL 互斥)
  ui/
    led_engine.c     60fps:状态环动画 / 用量条 / VU 条 / context 热力图,
                     1 或 4 块 matrix(S 形 + Z 序映射),全局限流
    display.c        ST7701(3-wire SPI 初始化借 RGB 线)+ LVGL9 + CST820 触摸
                     + TinyTTF(/spiffs/fonts/ui.ttf 自动加载);无屏时无头降级
  audio/audio.cpp    ES8311 全双工 16k/16bit;单 capture 任务分发 VU/录音/loopback
  drivers/qmi8658.cpp QMI8658C 最小驱动(±8g/±512dps)
  cases/             CAN / IMU 测试 case(audio case 在 audio.cpp 内)
  power/power.c      遥测任务(INA226/MP2760/电池 ADC 待实装)
```

## 测试控制台(USB Serial/JTAG,`idf.py monitor` 即可使用)

| 命令 | 功能 |
|---|---|
| `can_tx [count] [period_ms]` | 发送递增计数帧(ID 0x123),配合 PC `candump can0` |
| `can_rx [on\|off]` | 总线监听打印(联调协议帧时请关闭,与 comm_twai 竞争收包) |
| `can_status` | TWAI 状态/错误计数 |
| `audio_rec [sec]` | 录音到 PSRAM(≤30s) |
| `audio_play` | 回放 PSRAM 录音 |
| `audio_rec_sd <name> [sec]` | 录音到 `/sdcard/<name>.wav` |
| `audio_player <path>` | 播放 WAV(`/sdcard` 或 `/spiffs`,PCM16) |
| `audio_loop [on\|off]` | MIC→SPK 直通 |
| `audio_vol <0-100>` / `tone <0-3>` | 音量 / 提示音 |
| `imu` / `imu_vis [on\|off]` | 单次读取 / 持续可视化(LCD 水平仪+6 轴条,无屏转日志) |
| `sd [mount\|umount]` / `ls [path]` | SD 卡管理 / 列目录 |
| `lang [zh\|en]` | UI 语言切换(NVS 持久化) |

## 任务栈一览(C++ 模块注意事项)

| 任务 | 栈 | 备注 |
|---|---|---|
| LVGL(esp_lvgl_port) | 10240 | TinyTTF 栅格化在此任务 |
| console REPL | 8192 | 命令函数直接跑在该任务(audio_play 会阻塞至放完) |
| audio player | 8192 | vfs + WAV 解析 |
| audio capture | 6144 | 文件写入路径 |
| imu_vis / disp_text | 6144 | LVGL 调用 + float printf |
| can_tx/can_rx | 4096 | 纯驱动 + log |

新代码用 C++17,跨 C 边界一律 `extern "C"`;FreeRTOS 任务入口避免抛异常
(工程未开启 C++ 异常),栈上不放大缓冲(>512B 一律静态或堆)。

## 桥接联调(无硬件外设也可跑通)

```bash
# PC 侧(先建好 AP)
cd ../host && pip install -e . && agentind run -s demo -v
```

LED/显示/音频/IMU 对硬件的依赖都有运行时探测,裸 ESP32-S3 模组即可联调
Wi-Fi 链路 + 协议 + 状态机;文本流降级输出到 monitor 日志。
