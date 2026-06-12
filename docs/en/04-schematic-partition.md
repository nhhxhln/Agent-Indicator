# Agent Indicator — Schematic Design & Partitioning

> Rev 0.2 · 中文版: [../04-schematic-partition.md](../04-schematic-partition.md)
> One main board serves all four enclosure variants; LED carriers (matrix/circle/bar) are simple separate PCBs on cables.

## 1. Schematic Sheets (8)

| Sheet | Name | Contents | Key nets |
|---|---|---|---|
| 1 | INPUT-PD | USB-C (PD) + CH224K + ESD; XT30 reverse protection/TVS; LM5050-1×2 ORing | VPD, VIN, VIN_SEL |
| 2 | CHARGER | MP2760 + inductor/sense; S-8254A + dual NMOS; HY2213×3; cell dividers | VSYS, VBAT, BAT_ADC1-3 |
| 3 | DCDC | TPS56637 (VLED) + PMOS; MP2315S (5V0); TPS2121 (VUSB mux); SGM2212 (VDD); VAUDIO filter; INA226 | VLED, 5V0, 5V0_SYS, VDD, VAUDIO |
| 4 | MCU | ESP32-S3-WROOM-1-N16R8; reset/BOOT buttons; strapping; TCA9554 | EN, IO0, I2C_SDA/SCL |
| 5 | LCD-TOUCH | 40P FPC (ST7701 RGB); init-SPI sharing jumpers; backlight driver; CST820 FPC | LCD_D0-15, PCLK, HS, VS, DE |
| 6 | LED | SN74AHCT125 ×2 channels; VLED PMOS switch; 3× SH1.0-3P with 33Ω series + ESD | LED_M_DIN, LED_AUX_DIN |
| 7 | AUDIO | ES8311 + MEMS mics ×2 (analog diff) + NS4150B + SPK; PA_EN | I2S_BCLK/WS/DO/DI |
| 8 | EXPAND | TJA1051T/3 + CAN header + term jumper; microSD (1-bit); QMI8658C; Qwiic; USB-C data | TWAI_TX/RX, SD_CLK/CMD/D0 |

## 2. ESP32-S3 Pinmap v0.1

N16R8: GPIO35/36/37 taken by octal PSRAM; GPIO19/20 are USB D-/D+.

| GPIO | Function | Net | Notes |
|---|---|---|---|
| 0 | SD_CMD | SD_CMD | strapping (BOOT), 10k pull-up compatible with SD CMD |
| 1 | I2S_BCLK | I2S_BCLK | ES8311 MCLK-from-SCLK mode saves the MCLK pin |
| 2 | SD_CLK | SD_CLK | |
| 3 | LCD_G4 | LCD_G4 | strapping (JTAG sel), output-only is safe |
| 4–7 | LCD_B0–B3 | LCD_B0..3 | |
| 8 | LCD_G3 | LCD_G3 | |
| 9–13 | LCD_R0–R4 | LCD_R0..4 | |
| 14 | LCD_PCLK | LCD_PCLK | |
| 15 | LCD_B4 | LCD_B4 | |
| 16–18 | LCD_G0–G2 | LCD_G0..2 | |
| 19/20 | USB D-/D+ | USB_DN/DP | TinyUSB (vendor+CDC) |
| 21 | I2S_WS | I2S_WS | |
| 33 | I2S_DOUT | I2S_DO | → ES8311 SDIN |
| 34 | I2S_DIN | I2S_DI | ← ES8311 SDOUT |
| 38 | I2C_SDA | I2C_SDA | CST820/QMI8658C/TCA9554/INA226/MP2760, 4.7k pull-ups |
| 39 | I2C_SCL | I2C_SCL | |
| 40 | LED_MATRIX | LED_M_DIN | RMT CH0 via AHCT125 |
| 41 | LED_AUX | LED_AUX_DIN | RMT CH1 (circle+usage+mic chain) |
| 42 | SD_D0 | SD_D0 | 1-bit SDMMC |
| 43 | TWAI_TX | TWAI_TX | swappable for LCD_BL_PWM if CAN unused (solder option) |
| 44 | TWAI_RX | TWAI_RX | |
| 45 | LCD_VSYNC | LCD_VS | strapping (VDD_SPI), default pull-down is fine |
| 46 | LCD_HSYNC | LCD_HS | strapping (ROM log), output only |
| 47 | LCD_DE | LCD_DE | |
| 48 | LCD_G5 | LCD_G5 | G uses 6 bits in RGB565 |

**All 45 usable GPIOs are consumed**; slow controls go through TCA9554:

| TCA9554 | Signal |
|---|---|
| P0 | LCD_RST |
| P1 | TP_RST (CST820) |
| P2 | LCD_BL_EN |
| P3 | PA_EN (NS4150B) |
| P4 | LED_PWR_EN (VLED PMOS) |
| P5 | LCD_SPI_CS (ST7701 3-wire init SPI; SCK/SDA borrow LCD lines, released after init) |
| P6 | CHG_INT (MP2760, input) |
| P7 | TP_INT (input; touch is polled at 20ms, INT is a wake assist) |

I2C address map: CST820 0x15 · TCA9554 0x20 · QMI8658C 0x6B · INA226 0x40 · MP2760 0x5C (verify) · 24C02 (variant-D tiles) 0x50-0x53.

## 2.1 LCD 40P FPC Pinout (actual module spec)

Module: ST7701 + CST820, outline 73.03 × 76.48 × **2.34mm**, AA 70.13×70.13, SPI/RGB dual mode.

| FPC pin | Signal | Connection |
|---|---|---|
| LED_A / LED_K ×2 | backlight anode/cathodes | backlight driver (5V0, ~40mA), gated by BL_EN |
| GND ×3 | ground | — |
| VCI | panel supply | VDD 3.3V |
| RESET | panel reset | TCA9554 P0 (LCD_RST) |
| NC ×2 | — | — |
| SDA / SCK / CS | init SPI (9-bit 3-wire) | SDA←LCD_G0 net, SCK←LCD_PCLK net, CS←TCA9554 P5; the panel has dedicated SPI pins — only the MCU side borrows RGB GPIOs |
| PCLK / DE / VSYNC / HSYNC | RGB timing | GPIO14 / 47 / 45 / 46 |
| DB0–DB17 | 18-bit RGB data | RGB565 wiring: R4..0←DB17..13, G5..0←DB11..6, B4..0←DB5..1; **DB0 & DB12 to GND** (standard 666→565 drop, confirm against the panel datasheet) |
| TP_INT / TP_SDA / TP_SCL / TP_RESET / TP_VCI | CST820 touch | TCA9554 P7 / I2C_SDA / I2C_SCL / TCA9554 P1 / VDD |

## 3. ST7701 Init Path

The RGB panel needs its init sequence over 3-wire 9-bit SPI first. Borrowed lines
(same as Espressif EV boards): `SPI_SCK ← LCD_PCLK`, `SPI_SDA ← LCD_G0`,
`CS ← TCA9554 P5`. Bit-banged during init, then the pins are reconfigured to the
LCD_CAM RGB peripheral. See the two-stage init in `display.c`.

## 4. LED Carrier Boards

| Board | Contents | Interface |
|---|---|---|
| Matrix 8×8 | 64×WS2812B, 64×64mm, corner mounting holes; DIN/DOUT on 2.54-3P both sides for 4-tile S-cascade | VLED/GND/DATA |
| Circle | 24×WS2812B ring ID75/OD85, hollow center for the LCD FPC | same |
| Bar family | per-variant lengths, SH1.0-3P both ends (chainable) | same |

Each board: 33Ω series at DIN + 104 per LED + 220µF at the inlet.

## 5. Layout Zones (main board, est. 90×70mm, 4 layers)

```
┌─────────────────────────────────────────┐
│ [XT30][CAN] Sheet1/2 power   [USB-C PD] │  ← rear edge: all external I/O
│  inductors/MOS away ▼        [USB-C data]│
│ Sheet3 DCDC ──┐   ┌─ Sheet4 MCU (antenna │
│  VLED high-A  │   │   at edge, keepout)  │
│ ──────────────┴───┴───────────────────── │
│ Sheet7 AUDIO (away from DCDC) Sheet5/6/8 │
│  short mic traces            FPC/LED/SD  │
└─────────────────────────────────────────┘
```

- VLED 5A path: 2oz copper, ≥4mm traces, buck output straight to the LED connector.
- Wi-Fi antenna keepout ≥15×6mm, nothing underneath.
- Mic analog guarded with ground, away from PCLK (16MHz) and buck switch nodes.
- 16 RGB data lines length-matched (±5mm is fine at 16MHz), group-guarded.
