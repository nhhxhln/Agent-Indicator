# 06 · RGB Bridge — 用第二颗 ESP32-S3 省下主板 RGB 引脚

## 为什么需要
ST7701S 480×480 是 **RGB 并口屏**:RGB565 要 16 数据 + 4 同步(PCLK/DE/VSYNC/HSYNC)
+ 3 线 SPI 初始化 ≈ **23 根 GPIO**。主板(Agent Indicator)引脚紧张,直接驱动会吃光 GPIO。

**方案**:用一颗**专职的第二颗 ESP32-S3 当"桥片"**驱动 RGB 屏,主板只用 **4 根 SPI**
把画面帧推给桥片。主板因此**省下约 20 个 GPIO**。

```
  主板 ESP32-S3 ──(4 线 SPI 主)──▶ 桥片 ESP32-S3 ──(RGB565 并口 23 线)──▶ ST7701S 480×480
   RgbBridgeHost                      RgbBridge
```

## 两个工程
| 工程 | 角色 | 关键文件 |
|---|---|---|
| `RgbBridge/` | 桥片(SPI 从):收帧 + 驱动 ST7701S + 自测点屏 | `rgb_panel.c`(ST7701S/RGB)、`patterns.c`(自测)、`link_spi.c`(收帧) |
| `RgbBridgeHost/` | 主机(SPI 主):按协议推帧 | `rgb_bridge_host.c`(`push_line`/`push_frame`)、`app_main.c`(彩条测试) |

## 硬件约束(必读)
- **ESP32-S3 RGB 并口最多 16 数据线 = RGB565**,做不了 24 位 RGB888 并口。
  → 面板侧固定 **RGB565 输出**;主机推 **565 或 888 都收**,888 在桥片上转 565。
- 桥片需 **PSRAM**(RGB 双帧缓冲 + 暂存,占用约 1.84MB / 8MB)。
- ST7701S 厂商序列里 `COLMOD=0x66`(RGB666 18bit),16 根数据线接面板**高位**即可
  (RGB565→666 各色丢 1 LSB,肉眼无感)。

## 桥片引脚(`RgbBridge/main/board.h`,按转接板实际接线改)
| 用途 | GPIO |
|---|---|
| RGB565 数据 B0-4 / G0-5 / R0-4 | 4,5,6,7,8 / 9,10,11,12,13,14 / 15,16,17,18,21 |
| PCLK / DE / VSYNC / HSYNC | 39 / 40 / 41 / 42 |
| ST7701 初始化 3 线 SPI:CS / SCL / SDA | 45 / 47 / 48 |
| RST / 背光 BL | 46 / 44 |
| 主机 SPI 从机:CS / CLK / MOSI / MISO | 1 / 2 / 38 / 43 |

## 主机↔桥片接线(4 根 + 共地)
| 主机 RgbBridgeHost | → | 桥片 RgbBridge |
|---|---|---|
| PIN_CS 15 | → | HOST_SPI_CS 1 |
| PIN_CLK 16 | → | HOST_SPI_CLK 2 |
| PIN_MOSI 17 | → | HOST_SPI_MOSI 38 |
| PIN_MISO 18 | → | HOST_SPI_MISO 43(可不接) |

## 线协议(主机推帧)
SPI 模式 0;**每个 SPI 事务 = 一行包**:

| 字节 | 含义 |
|---|---|
| [0] | `0xA9` 魔数(帧同步,错位包丢弃) |
| [1] | 格式:`0`=RGB565 小端,`1`=RGB888 |
| [2..3] | 行号 y(小端 u16) |
| [4..] | 该行 480 像素(565:960B / 888:1440B) |

主机连续推 y=0..479;桥片收到 `y==479` 即整帧刷新上屏。收到首个合法包后桥片**自测停止**。
默认链路 20MHz(`LINK_CLK_HZ`)≈ 5fps@565,可上调。

## 面板初始化与时序(已按本屏手册配好)
- **初始化序列**:`board.h` 的 `USE_VENDOR_INIT`,`1`=面板厂商专属序列
  (`rgb_panel.c` 的 `s_vendor_init[]`,Gamma/VCOM=32/VGH=15V/AVDD=6.6V),`0`=组件通用默认。偏色/不亮两版都试。
- **RGB 时序**(来自面板手册 480RGB×480 Timing Table,取 Typ):
  PCLK **20MHz**,HSYNC 脉宽 **8**/后肩 **60**/前肩 **60**,VSYNC 脉宽 **4**/后肩 **20**/前肩 **20**。
  (手册 Thw 处 Typ20 与 Max8 矛盾,取 Max=8。)

## 编译 / 烧录 / 验证
```bash
. /home/zhe/Code/esp-idf/export.sh
# 桥片
cd RgbBridge && idf.py set-target esp32s3 && idf.py build flash monitor
# 主机(另一颗 S3)
cd ../RgbBridgeHost && idf.py set-target esp32s3 && idf.py build flash monitor
```
1. **单烧桥片** → 屏应循环显示纯色/彩条/渐变/棋盘/白边框(确认时序/初始化对);
2. **再烧主机 + 接 4 线** → 桥片自测让位,屏上出现**移动彩条**(全链路通);
3. 之后把 `rgb_bridge_push_frame()` 接到真实画面源(LVGL 帧缓冲)即可。

## 后续可优化(v1 已能用)
- 双缓冲 + DMA 链 + 握手 GPIO 提帧率;
- 脏矩形/分块刷新(只推变化区域);
- MISO 回状态/帧号做流控。
