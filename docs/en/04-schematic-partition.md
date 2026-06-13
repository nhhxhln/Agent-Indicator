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
| 5 | LCD-TOUCH | 40P FPC (ST7701 RGB); init-SPI sharing jumpers; backlight driver; CST836U FPC | LCD_D0-15, PCLK, HS, VS, DE |
| 6 | LED | SN74AHCT125 ×2 channels; VLED PMOS switch; 3× SH1.0-3P with 33Ω series + ESD | LED_M_DIN, LED_AUX_DIN |
| 7 | AUDIO | ES8311 + MEMS mics ×2 (analog diff) + NS4150B + SPK; PA_EN | I2S_BCLK/WS/DO/DI |
| 8 | EXPAND | TJA1051T/3 + CAN header + term jumper; microSD (1-bit); QMI8658C; Qwiic; USB-C data | TWAI_TX/RX, SD_CLK/CMD/D0 |

## 2. ESP32-S3 Pinmap v0.2 (ESP32-S3-DevKitC-1 N16R8)

> ⚠ **N16R8 constraint**: GPIO26-32 = quad flash, **GPIO33-37 = octal PSRAM**,
> all unusable (v0.1 wrongly placed I2S on 33/34, fixed in v0.2). Only 0-18, 21,
> 38-48 (**29 pins**) are free; GPIO19/20 are kept for native USB (USB-Serial-JTAG:
> log + flashing + REPL).

Table below is the default **Profile A (Display + Audio + Backlight)**, physically
bring-up-able on DevKitC-1:

| GPIO | Function | Net | Notes |
|---|---|---|---|
| 0 | (SD_CMD) / spare | — | strapping (BOOT); no SD in Profile A |
| 1 | I2S_BCLK | I2S_BCLK | ES8311 MCLK-from-SCLK mode saves the MCLK pin |
| 2 | (SD_CLK) / spare | — | no SD in Profile A |
| 3 | LCD_G4 | LCD_G4 | strapping (JTAG sel), output-only is safe |
| 4–7 | LCD_B0–B3 | LCD_B0..3 | |
| 8 | LCD_G3 | LCD_G3 | |
| 9–13 | LCD_R0–R4 | LCD_R0..4 | |
| 14 | LCD_PCLK | LCD_PCLK | |
| 15 | LCD_B4 | LCD_B4 | |
| 16–18 | LCD_G0–G2 | LCD_G0..2 | |
| 19/20 | USB D-/D+ | USB_DN/DP | **USB-Serial-JTAG** (log/flash/REPL); exclusive with TinyUSB device |
| 21 | I2S_WS | I2S_WS | |
| 38 | I2C_SDA | I2C_SDA | CST836U/QMI8658C/TCA9554/INA226/MP2760/SHT4x/BMP280/PCF8563, 4.7k pull-ups |
| 39 | I2C_SCL | I2C_SCL | |
| 40 | LED_MATRIX | LED_M_DIN | RMT CH0 via AHCT125 |
| 41 | LED_AUX | LED_AUX_DIN | RMT CH1 (circle+usage+mic chain) |
| 42 | **I2S_DOUT** | I2S_DO | → ES8311 SDIN (v0.2: was 33). Shares SD_D0 if SD enabled |
| 43 | **LCD_BL_PWM** | BL_PWM | LEDC backlight dimming → constant-current IC DIM. Shares TWAI_TX if CAN enabled |
| 44 | **I2S_DIN** | I2S_DI | ← ES8311 SDOUT (v0.2: was 34). Shares TWAI_RX if CAN enabled |
| 45 | LCD_VSYNC | LCD_VS | strapping (VDD_SPI), default pull-down is fine |
| 46 | LCD_HSYNC | LCD_HS | strapping (ROM log), output only |
| 47 | LCD_DE | LCD_DE | |
| 48 | LCD_G5 | LCD_G5 | G uses 6 bits in RGB565 |

### Profiles & trade-offs (DevKitC-1)

The 480×480 RGB565 panel eats 20 GPIOs; with I2C/LED/I2S/backlight the 29 free pins
are full — **on-board SD and CAN can't both be added simultaneously**. Kconfig switches:

| Feature | Profile A (default) | Enable |
|---|---|---|
| LCD + touch + I2C sensors | ✅ | — |
| WS2812 (matrix/circle/bar) | ✅ | — |
| Audio (ES8311 full-duplex) | ✅ | — |
| Backlight PWM dimming | ✅ | — |
| On-board microSD | ❌ (D0=42 clashes I2S) | `CONFIG_AGENTIND_ENABLE_SD`, disable audio or re-route |
| CAN/TWAI | ❌ (43/44 clash backlight/I2S) | `CONFIG_AGENTIND_ENABLE_CAN`, re-route TWAI via GPIO matrix |

> A production bare-module board wanting full features needs a **serial RGB (SPI/QSPI)
> panel** or IO expanders / shift registers to free up data lines; DevKitC-1 prototyping
> validates display+audio+LEDs+sensors via Profile A.

## 2.2 LCD Backlight Constant-Current Driver & Dimming

**LED_A/LED_K are the anode/cathode of the panel's internal backlight LED string.**

**Confirmed backlight spec**: **If = 80mA, Vf typ 3.2V (Min 2.8 / Max 3.3V)**.
Vf < 5V0 → **no boost needed**; a low-voltage linear constant-current source suffices
(single string / all-parallel).

- **Default**: a 5V0-input linear CC source at 80mA:
  - CC IC: **NSI45080** (fixed 80mA) or **BCR401U / AMC7140** (settable), SOT-23/89, minimal externals;
  - dropout loss (5.0−3.2)×0.08 ≈ **0.144W**, small package + copper pour is enough;
  - `EN` ← TCA9554 `EXP_LCD_BL_EN` (slow on/off); `PWM/CTRL` ← MCU `GPIO43` (LEDC 2kHz PWM).
- **Cheaper**: drive via a series resistor — R ≈ (5.0−3.2)/0.08 ≈ **22Ω**, ~0.14W (0805/1206).
  Downside: current drifts with Vf/temperature. A 0Ω jumper selects "CC IC / resistor".
- **Dimming**: firmware `display_set_backlight(pct)` (LEDC 8-bit duty); the Lighting page
  brightness slider drives both LED global brightness and the LCD backlight.

## 2.3 DevKitC-1 Adapter Board

The prototype is an **ESP32-S3-DevKitC-1 (N16R8)** on an adapter carrying TCA9554,
AHCT125 level shifter, ES8311+NS4150B audio, backlight CC IC, the LCD 40P FPC socket,
WS2812 SH1.0 outlets, a Qwiic I2C header (IMU/SHT4x/BMP280/PCF8563) and the power DCDCs.
See §2 for the pinmap and Profile trade-offs.

**All 29 usable GPIOs are consumed**; slow controls go through TCA9554:

| TCA9554 | Signal |
|---|---|
| P0 | LCD_RST |
| P1 | TP_RST (CST836U) |
| P2 | LCD_BL_EN |
| P3 | PA_EN (NS4150B) |
| P4 | LED_PWR_EN (VLED PMOS) |
| P5 | LCD_SPI_CS (ST7701 3-wire init SPI; SCK/SDA borrow LCD lines, released after init) |
| P6 | CHG_INT (MP2760, input) |
| P7 | TP_INT (input; touch is polled at 20ms, INT is a wake assist) |

I2C address map: CST836U 0x15 · TCA9554 0x20 · QMI8658C 0x6B · INA226 0x40 · MP2760 0x5C (verify) · 24C02 (variant-D tiles) 0x50-0x53.

## 2.1 LCD 40P FPC Pinout (actual module spec)

Module: ST7701 + CST836U, outline 73.03 × 76.48 × **2.34mm**, AA 70.13×70.13, SPI/RGB dual mode.

| FPC pin | Signal | Connection |
|---|---|---|
| LED_A / LED_K ×2 | backlight anode/cathodes | via a constant-current backlight driver (see §2.2), not a bare resistor |
| GND ×3 | ground | — |
| VCI | panel supply | VDD 3.3V |
| RESET | panel reset | TCA9554 P0 (LCD_RST) |
| NC ×2 | — | — |
| SDA / SCK / CS | init SPI (9-bit 3-wire) | SDA←LCD_G0 net, SCK←LCD_PCLK net, CS←TCA9554 P5; the panel has dedicated SPI pins — only the MCU side borrows RGB GPIOs |
| PCLK / DE / VSYNC / HSYNC | RGB timing | GPIO14 / 47 / 45 / 46 |
| DB0–DB17 | 18-bit RGB data | RGB565 wiring: R4..0←DB17..13, G5..0←DB11..6, B4..0←DB5..1; **DB0 & DB12 to GND** (standard 666→565 drop, confirm against the panel datasheet) |
| TP_INT / TP_SDA / TP_SCL / TP_RESET / TP_VCI | CST836U touch | TCA9554 P7 / I2C_SDA / I2C_SCL / TCA9554 P1 / VDD |

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
- Mic analog guarded with ground, away from PCLK (20MHz) and buck switch nodes.
- 16 RGB data lines length-matched (±5mm is fine at 20MHz), group-guarded.
