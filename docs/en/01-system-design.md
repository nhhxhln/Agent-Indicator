# Agent Indicator — System Design Overview

> LLM status indicator desk gadget · Rev 0.2 · 2026-06
> 中文版:[../01-system-design.md](../01-system-design.md)
> Companion docs: [02-power-design.md](02-power-design.md) Power · [03-industrial-design.md](03-industrial-design.md) Industrial design · [04-schematic-partition.md](04-schematic-partition.md) Schematics

## 1. Feature Definition

| Feature | Carrier | Description |
|---|---|---|
| LLM usage display | RGB Bar (20-LED vertical, segmented) | Multi-slot percentage bars: session usage, 5h/weekly limits |
| LLM status display | RGB Circle (24 LEDs) | Animations for idle / thinking / responding / tool-use / error |
| LLM context display | RGB Matrix (8×8; firmware supports 4 tiles as 16×16) | Context-window heat map / category color blocks |
| LLM input/output | LCD (ST7701 480×480 + CST836U touch) | Scrolling prompt/response digest, touch paging |
| Mic level | RGB Bar (long horizontal) | Local mic VU meter / tone visualization |
| Tones / voice pickup | I2S codec + PA + MEMS mic | Task-done chimes, voice input level |

Expansion interfaces: SDMMC (assets/recordings), CAN/TWAI (bus access), USB Device (custom EP + CDC), I2C (touch + sensors).

## 2. System Block Diagram

```
                        ┌────────────────────────────────────────────┐
   Linux PC (Host)      │              Agent Indicator               │
  ┌──────────────┐      │  ┌──────────────────────────────────────┐  │
  │ agentind     │ WiFi │  │           ESP32-S3-WROOM-1-N16R8     │  │
  │ (Python      │◄────►│  │                                      │  │
  │  bridge)     │ CAN  │  │ RGB-LCD16bit ──► ST7701 480×480 LCD  │  │
  │  - sources   │◄────►│  │ I2C ──► CST836U / QMI8658C / TCA9554  │  │
  │  - protocol  │ USB  │  │            / INA226 / MP2760         │  │
  │  - transports│◄────►│  │ RMT×2 ──► WS2812B Matrix / Ring+Bars │  │
  └──────────────┘      │  │ I2S ──► ES8311 ──► NS4150B ──► SPK   │  │
                        │  │              ▲MIC                    │  │
                        │  │ SDMMC 1-bit ──► microSD              │  │
                        │  │ TWAI ──► TJA1051T/3 ──► CAN          │  │
                        │  │ USB-OTG ──► Type-C (data, separate   │  │
                        │  │             from the PD port)        │  │
                        │  └──────────────────────────────────────┘  │
                        │  Power: PD(15V) / XT30(12-24V) / 3S 18650  │
                        └────────────────────────────────────────────┘
```

## 3. Component Selection

### 3.1 Main Controller

| Item | Choice | Rationale |
|---|---|---|
| MCU module | **ESP32-S3-WROOM-1-N16R8** | 16MB flash + 8MB octal PSRAM. The 480×480×RGB565 frame buffer (~460KB) must live in PSRAM; LCD_CAM drives the RGB panel directly; native USB-OTG; TWAI; RMT for WS2812; Wi-Fi |

Note: octal PSRAM on the N16R8 occupies GPIO35/36/37 — see the pinmap in doc 04.

### 3.2 Peripheral ICs

| Function | Choice | Alternative | Notes |
|---|---|---|---|
| CAN transceiver | **TJA1051T/3** | SIT1051T | /3 variant has 3.3V VIO, no level shifting; 5V from 5V0 |
| Audio codec | **ES8311** | ES8388 (stereo) | Mono ADC+DAC, I2S + I2C control, native `esp_codec_dev` support; can derive MCLK from SCLK (saves one GPIO) |
| Audio PA | **NS4150B** | MAX98357 (pure I2S) | 3W class-D, same pairing as Espressif dev boards; EN on the IO expander |
| MIC | **MSM261 (analog MEMS) → ES8311 MIC_IN** | INMP441 (I2S digital) | Analog input via codec saves I2S lines; VU computed in firmware (RMS) |
| IMU | **QMI8658C** | LSM6DS3TR-C | 6-axis, I2C, tap/raise interrupts; cheap and well documented; used for tap interaction and orientation flip |
| Touch | **CST836U** (with panel) | — | I2C 0x15, same protocol family as CST816, works with `esp_lcd_touch_cst816s` |
| IO expander | **TCA9554** | PCA9554 | LCD_RST / TP_RST / PA_EN / BL_EN / LED_PWR_EN and other slow controls; relieves GPIO pressure |
| Current monitor | **INA226** | INA219 | High-side on VSYS; battery/power telemetry |
| LED level shifter | **SN74AHCT125** | 74HCT245 | 3.3V→5V data; WS2812B at 5V needs VIH=3.5V, shifting is mandatory |

Power-chain parts (CH224K / MP2760 / S-8254A / HY2213 / TPS56637 / MP2315S / TPS2121 / SGM2212): see [02-power-design.md](02-power-design.md).

### 3.3 Display & LEDs

| Item | Parameters |
|---|---|
| LCD | ST7701S, 480×480, 16-bit RGB565 parallel + 3-wire SPI init (SPI lines borrowed from RGB data lines, same trick as Espressif reference designs); outline 73.03×76.48mm, AA 70.13×70.13mm |
| RGB Matrix | WS2812B 8×8, 64×64mm, board-to-board/cable cascade; firmware supports 1 or 4 tiles (16×16), configurable serpentine scan |
| RGB Circle | WS2812B ×24, ID 75 / OD 85mm |
| RGB Bar | Length per enclosure variant: usage 20LED@41.5mm vertical; mic bar 48LED@99mm or 64LED@132mm (see doc 03) |

## 4. Module Design

### 4.1 LED subsystem

- **Driver**: RMT ×2 channels (`led_strip` component, DMA).
  - CH0 `LED_MATRIX`: matrix chain (1–4 tiles, 64–256 px).
  - CH1 `LED_AUX`: circle(24) → usage bar(20) → mic bar(48/64), one chain with logical segments.
- **Refresh**: 60 fps animation engine; a 172-LED chain @800kHz ≈ 5.2ms, ample headroom.
- **Current limiting**: firmware global limiter — sums theoretical current per frame (16mA per channel), scales the whole frame down when over the VLED budget (4.5A). Essential for 16×16.
- **Power**: VLED 5V/5A, LED_PWR_EN (TCA9554) gates a PMOS; fully unpowered in sleep.

### 4.2 LCD subsystem

- `esp_lcd` RGB panel + `esp_lcd_st7701` (managed component), dual bounce buffers against PSRAM bandwidth jitter; PCLK from 20MHz.
- LVGL 9 (`esp_lvgl_port`): I/O text stream page, status page, settings; CST836U swipe paging.
- Backlight: BL_EN (TCA9554) on/off + content-adaptive dimming; hardware PWM dimming possible by sacrificing TWAI_TX (noted in pinmap).

### 4.3 Audio subsystem

- ES8311: I2S slave, internal MCLK from BCLK ("MCLK from SCLK" mode), 16kHz/16-bit capture, 44.1kHz tone playback.
- Tones: WAV from SD or embedded in flash.
- Mic bar: 20ms-window RMS → dB → gradient on the mic bar, closed loop on-device.

### 4.4 Communication subsystem (three links, one protocol layer)

| Link | Implementation | Scenario |
|---|---|---|
| Wi-Fi | Device runs a WebSocket server (`esp_http_server`) + mDNS `_agentind._tcp`; the PC acts as AP, host discovers via zeroconf and connects | Primary link, wireless desk placement |
| CAN | TWAI 500kbps, 11-bit IDs, light segmentation for >8B (see §5.3); PC side USB-CAN + SocketCAN | Industrial / multi-device bus |
| USB | TinyUSB composite: vendor-class bulk EP (protocol frames) + CDC (logs) | Zero-config direct attach, firmware debug |

All three links listen simultaneously; the most recently active link gets telemetry replies.

### 4.5 Power management

- MP2760 (I2C) + INA226 + 3-cell voltage sense → SoC estimation, charge state; low battery = red breathing on the circle edge.
- Idle 5min: LCD dims, LEDs drop to a breathing standby; 30min: VLED off, LCD sleep, wake on touch/IMU tap.

## 5. Protocol (host ↔ device)

### 5.1 Frame format (WS / USB / CDC)

```
┌──────┬─────┬──────┬──────┬─────────┬─────────┬───────┐
│ 0xA9 │ VER │ TYPE │ FLAGS│ LEN(LE) │ PAYLOAD │ CRC16 │
│  1B  │ 1B  │  1B  │  1B  │   2B    │  ≤512B  │  2B   │
└──────┴─────┴──────┴──────┴─────────┴─────────┴───────┘
```
- VER=0x01; CRC16-CCITT over VER..PAYLOAD; one message per WS binary frame.

### 5.2 Message types

| TYPE | Dir | Name | Payload |
|---|---|---|---|
| 0x01 | H→D | STATE | `u8 state, u8 detail`. state: 0 IDLE / 1 CONNECTING / 2 THINKING / 3 RESPONDING / 4 TOOL_USE / 5 WAITING_USER / 6 ERROR / 7 RATE_LIMITED / 8 OFFLINE |
| 0x02 | H→D | USAGE | `u8 n, n×{u8 slot, u8 percent}`. slot: 0 session / 1 5h limit / 2 weekly / 3 cost |
| 0x03 | H→D | CONTEXT | `u32 used, u32 total, u8 n, n×{u8 category, u32 tokens}`. category: system/tools/mcp/memory/messages/free |
| 0x04 | H→D | TEXT | `u8 stream(0 in/1 out), u8 op(0 append/1 replace/2 clear), utf8[]` |
| 0x05 | H→D | TONE | `u8 tone_id` (0 done / 1 attention / 2 error / 3 boot) |
| 0x06 | H→D | CONFIG | `u8 key, u32 value`. key: 0 brightness / 1 matrix tiles (1\|4) / 2 orientation / 3 UI language (0 en, 1 zh) / 4 light fx ([15:8]=mode 0-5, [7:0]=speed) / 5 fx color 0xRRGGBB (send before key=4) |
| 0x80 | D→H | TELEMETRY | `u16 vbat_mV ×3cell, i16 ibat_mA, u8 soc, u8 chg_state, u8 src(0 bat/1 pd/2 xt30)` |
| 0x81 | D→H | INPUT | `u8 kind(0 touch/1 imu_tap/2 imu_shake), u8 arg` |
| 0x82 | D→H | MIC_LEVEL | `u8 db` (optional subscription) |

### 5.3 CAN mapping

11-bit ID = `0x500 | TYPE & 0x1F`; payloads >7B are segmented: `byte0 = (seq<<4)|total`, first segment's byte1 is the original TYPE. TELEMETRY goes out as a single frame at `0x580`.

## 6. Software Deliverables

```
host/      Python package agentind: sources (Claude Code hooks/JSONL) → protocol → ws/can/usb
firmware/  ESP-IDF v5.2: comm (3 links) / proto / ui (matrix·ring·bar·lcd) / audio / power
           + storage (SPIFFS/SD) / console (test REPL) / cases (CAN·audio·IMU tests)
```

See each directory's README.

## 7. Firmware Platform Capabilities (added in v0.2)

| Capability | Implementation | Notes |
|---|---|---|
| File systems | SPIFFS (6MB `storage` partition → `/spiffs`) + microSD (1-bit SDMMC → `/sdcard`, hot-mountable) | LVGL accesses both via `LV_USE_FS_POSIX` drive letter `A:` |
| Vector fonts | LVGL TinyTTF (stb_truetype-based, **not FreeType**) | Auto-loads `/spiffs/fonts/ui.ttf` at boot; supports .ttf and .otf with TrueType outlines, **not CFF/PostScript .otf**, no hinting/kerning. For full OTF switch to `lv_freetype` + FreeType (~+600KB) |
| UI i18n | zh/en string tables in `ui/i18n.c`, persisted in NVS | Switch via console `lang zh\|en` or protocol CONFIG key=3; Chinese rendering requires a CJK TTF (built-in Montserrat has no CJK glyphs) |
| LVGL | LVGL9 + esp_lvgl_port, dual FB direct mode + bounce buffer anti-tearing | LVGL task stack 10KB (TinyTTF rasterization runs there) |
| Test console | esp_console REPL @ USB Serial/JTAG | Mutually exclusive with the TinyUSB vendor EP (same USB PHY); pick one for production |
| Test cases | CAN tx/rx; audio rec/replay/player-from-SD/rec-to-SD/loopback; IMU visualization | C++ (`cases/`), command list in firmware/README; degrades to log mode without LCD/peripherals |
| C++ support | New modules in C++17, `extern "C"` interop with the C framework | Explicit task stacks: LVGL 10K / REPL 8K / player 8K / capture 6K / imu_vis 6K / CAN 4K |

### 7.1 Audio data-flow design

A single capture task owns I2S RX: it always computes the VU level (mic bar), and
dispatches the same stream to recording (RAM/SD) or loopback depending on mode; the
TX side is serialized by a mutex (tone / player / loopback never run concurrently),
avoiding multi-task contention over I2S handles.
