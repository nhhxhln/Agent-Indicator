# Agent Indicator — 原理图设计与划分

> Rev 0.2 · English version: [en/04-schematic-partition.md](en/04-schematic-partition.md)
> 主板一块通吃 4 个外形方案,LED 载板(Matrix/Circle/Bar)为独立简单 PCB 经线缆接入。

## 1. 原理图页面划分(8 页)

| Sheet | 名称 | 内容 | 关键网络 |
|---|---|---|---|
| 1 | INPUT-PD | USB-C(PD) + CH224K + ESD;XT30 防反接/TVS;LM5050-1×2 ORing | VPD, VIN, VIN_SEL |
| 2 | CHARGER | MP2760 + 电感/采样电阻;S-8254A+双 NMOS;HY2213×3;电池分压采样 | VSYS, VBAT, BAT_ADC1-3 |
| 3 | DCDC | TPS56637(VLED)+ 前级 PMOS;MP2315S(5V0);TPS2121(VUSB mux);SGM2212(VDD);VAUDIO 滤波;INA226 | VLED, 5V0, 5V0_SYS, VDD, VAUDIO |
| 4 | MCU | ESP32-S3-WROOM-1-N16R8;复位/BOOT 按键;策略电阻;TCA9554 | EN, IO0, I2C_SDA/SCL |
| 5 | LCD-TOUCH | 40P FPC 座(ST7701 RGB);初始化 SPI 跳线借用 D 线;背光驱动;CST820 FPC | LCD_D0-15, PCLK, HS, VS, DE |
| 6 | LED | SN74AHCT125 电平转换 ×2 通道;VLED PMOS 开关;3×SH1.0-3P 出口 + 防护(33Ω 串阻 + ESD) | LED_M_DIN, LED_AUX_DIN |
| 7 | AUDIO | ES8311 + MEMS MIC×2(模拟差分)+ NS4150B + SPK 座;PA_EN | I2S_BCLK/WS/DO/DI |
| 8 | EXPAND | TJA1051T/3 + CAN 端子 + 终端跳线;microSD(1-bit);QMI8658C;Qwiic I2C 座;USB-C 数据口 | TWAI_TX/RX, SD_CLK/CMD/D0 |

## 2. ESP32-S3 Pinmap v0.1

N16R8:GPIO35/36/37 被 Octal PSRAM 占用;GPIO19/20 为 USB D-/D+。

| GPIO | 功能 | 网络 | 备注 |
|---|---|---|---|
| 0 | SD_CMD | SD_CMD | strapping(BOOT),上拉 10k,与 SD CMD 上拉兼容 |
| 1 | I2S_BCLK | I2S_BCLK | ES8311 MCLK-from-SCLK 模式,省 MCLK 脚 |
| 2 | SD_CLK | SD_CLK | |
| 3 | LCD_G4 | LCD_G4 | strapping(JTAG sel),仅作输出安全 |
| 4–7 | LCD_B0–B3 | LCD_B0..3 | |
| 8 | LCD_G3 | LCD_G3 | |
| 9–13 | LCD_R0–R4 | LCD_R0..4 | |
| 14 | LCD_PCLK | LCD_PCLK | |
| 15 | LCD_B4 | LCD_B4 | |
| 16–18 | LCD_G0–G2 | LCD_G0..2 | |
| 19/20 | USB D-/D+ | USB_DN/DP | TinyUSB(Vendor+CDC) |
| 21 | I2S_WS | I2S_WS | |
| 33 | I2S_DOUT | I2S_DO | → ES8311 SDIN |
| 34 | I2S_DIN | I2S_DI | ← ES8311 SDOUT |
| 38 | I2C_SDA | I2C_SDA | CST820/QMI8658C/TCA9554/INA226/MP2760,4.7k 上拉 |
| 39 | I2C_SCL | I2C_SCL | |
| 40 | LED_MATRIX | LED_M_DIN | RMT CH0,经 AHCT125 |
| 41 | LED_AUX | LED_AUX_DIN | RMT CH1(circle+usage+拾音链) |
| 42 | SD_D0 | SD_D0 | 1-bit SDMMC |
| 43 | TWAI_TX | TWAI_TX | 不用 CAN 时可改 LCD_BL_PWM(焊位二选一) |
| 44 | TWAI_RX | TWAI_RX | |
| 45 | LCD_VSYNC | LCD_VS | strapping(VDD_SPI),下拉默认即可 |
| 46 | LCD_HSYNC | LCD_HS | strapping(ROM log),仅输出 |
| 47 | LCD_DE | LCD_DE | |
| 48 | LCD_G5 | LCD_G5 | RGB565 时 G 占 6 位 |

**45/48 个 GPIO 全部用满**,慢速控制全部走 TCA9554:

| TCA9554 | 信号 |
|---|---|
| P0 | LCD_RST |
| P1 | TP_RST(CST820) |
| P2 | LCD_BL_EN |
| P3 | PA_EN(NS4150B) |
| P4 | LED_PWR_EN(VLED PMOS) |
| P5 | LCD_SPI_CS(ST7701 初始化 3-wire SPI,SCK/SDA 借用 LCD_PCLK 前的 D 线,初始化完成后释放) |
| P6 | CHG_INT(MP2760 中断,输入) |
| P7 | TP_INT(输入;触摸采用 20ms 轮询,INT 仅作唤醒辅助) |

I2C 地址表:CST820 0x15 · TCA9554 0x20 · QMI8658C 0x6B · INA226 0x40 · MP2760 0x5C(待核) · 24C02(D 方案模块)0x50-0x53。

## 2.1 LCD 40P FPC 引脚定义(实物规格)

模组:ST7701 + CST820,结构 73.03 × 76.48 × **2.34mm**,AA 70.13×70.13,SPI/RGB 双模。

| FPC 引脚 | 信号 | 接法 |
|---|---|---|
| LED_A / LED_K ×2 | 背光阳/阴极 | 背光驱动(5V0,~40mA),BL_EN 控制 |
| GND ×3 | 地 | — |
| VCI | 面板电源 | VDD 3.3V |
| RESET | 面板复位 | TCA9554 P0(LCD_RST) |
| NC ×2 | 空 | — |
| SDA / SCK / CS | 初始化 SPI(9-bit 3-wire) | SDA←LCD_G0 网络,SCK←LCD_PCLK 网络,CS←TCA9554 P5;面板有独立 SPI 脚,无需在面板侧复用,仅 MCU 侧借 RGB 引脚 |
| PCLK / DE / VSYNC / HSYNC | RGB 时序 | GPIO14 / 47 / 45 / 46 |
| DB0–DB17 | 18-bit RGB 数据 | RGB565 接法:R4..0←DB17..13,G5..0←DB11..6,B4..0←DB5..1;**DB0、DB12 接地**(666→565 标准降位,以屏厂 datasheet 终核) |
| TP_INT / TP_SDA / TP_SCL / TP_RESET / TP_VCI | CST820 触摸 | TCA9554 P7 / I2C_SDA / I2C_SCL / TCA9554 P1 / VDD |

## 3. ST7701 初始化链路说明

RGB 屏需先经 3-wire 9-bit SPI 写初始化序列。借用方式(Espressif EV-Board 同款):
`SPI_SCK ← LCD_PCLK 线`、`SPI_SDA ← LCD_D0 线`、`CS ← TCA9554 P5`。
初始化阶段以 GPIO 位带模拟 SPI,完成后重配为 LCD_CAM RGB 外设。固件中对应 `display.c` 的两段初始化。

## 4. LED 载板(独立小板)

| 板 | 内容 | 接口 |
|---|---|---|
| Matrix 8×8 | 64×WS2812B,64×64mm,四角安装孔;DIN/DOUT 在两侧 2.54-3P,支持 4 块 S 形级联 | VLED/GND/DATA |
| Circle | 24×WS2812B 环形 ID75/OD85,中空走 LCD FPC | 同上 |
| Bar 系列 | 按方案选长度,统一 SH1.0-3P 双端(可续接) | 同上 |

每板 DIN 入口处:33Ω 串阻 + 1×104 每灯去耦 + 入口 220µF。

## 5. 布局分区建议(主板,估 90×70mm 四层)

```
┌─────────────────────────────────────────┐
│ [XT30][CAN] Sheet1/2 功率区  [USB-C PD] │  ← 后缘:全部对外接口
│  电感/MOS 远离 ▼               [USB-C 数据]│
│ Sheet3 DCDC ──┐   ┌─ Sheet4 MCU(天线   │
│  VLED 大电流  │   │   朝板边,净空区)   │
│ ──────────────┴───┴──────────────────── │
│ Sheet7 AUDIO(远离 DCDC)  Sheet5/6/8    │
│  MIC 走线最短             FPC/LED/SD 座 │
└─────────────────────────────────────────┘
```

- VLED 5A 路径:2oz 铜 + ≥4mm 走线宽,buck 出口直达 LED 连接器。
- Wi-Fi 天线区净空 ≥15×6mm,下方禁布。
- MIC 模拟走线包地,远离 PCLK(16MHz)与 buck 开关节点。
- RGB LCD 16 根数据线等长(±5mm 内即可,16MHz 不敏感),整组包地。
