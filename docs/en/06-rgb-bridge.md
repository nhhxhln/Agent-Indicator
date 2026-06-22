# 06 · RGB Bridge — offload ST7701S RGB pins to a second ESP32-S3

## Why
ST7701S 480×480 is an **RGB parallel** panel: RGB565 needs 16 data + 4 sync
(PCLK/DE/VSYNC/HSYNC) + 3-wire SPI init ≈ **23 GPIOs**. The main board can't spare that.

**Solution:** a dedicated **second ESP32-S3 "bridge"** drives the RGB panel; the main board
pushes frames over just **4 SPI wires**, saving ~20 GPIOs on the main board.

```
  Main ESP32-S3 ──(4-wire SPI master)──▶ Bridge ESP32-S3 ──(RGB565 parallel 23)──▶ ST7701S 480×480
   RgbBridgeHost                            RgbBridge
```

## Two projects
| Project | Role |
|---|---|
| `RgbBridge/` | Bridge (SPI slave): receive frames + drive ST7701S + standalone self-test |
| `RgbBridgeHost/` | Host (SPI master): push frames (`push_line`/`push_frame`) + moving-bars test |

## Hardware constraints
- ESP32-S3 RGB bus is **max 16 data lines = RGB565**; true 24-bit RGB888 parallel is not
  possible on S3. Panel output is fixed RGB565; host may send **565 or 888** (888 → 565 on bridge).
- Bridge needs **PSRAM** (~1.84 MB for RGB double buffer + staging).
- Vendor init uses `COLMOD=0x66` (RGB666 18-bit); wire the 16 lines to the panel's high bits.

## Wire protocol (one SPI transaction = one line)
`[0]=0xA9 magic  [1]=fmt(0=RGB565LE,1=RGB888)  [2..3]=y (LE u16)  [4..]=480 px`
Bridge refreshes the panel when it receives `y==479`. First valid packet stops the self-test.

## Init & timing (already set for this panel)
- `USE_VENDOR_INIT` in `board.h`: `1`=panel vendor sequence (`s_vendor_init[]`), `0`=component default.
- RGB timing from the panel datasheet (Typ): PCLK 20 MHz; HSYNC pw 8 / bp 60 / fp 60;
  VSYNC pw 4 / bp 20 / fp 20.

## Build / verify
```bash
. /home/zhe/Code/esp-idf/export.sh
cd RgbBridge      && idf.py set-target esp32s3 && idf.py build flash monitor
cd RgbBridgeHost  && idf.py set-target esp32s3 && idf.py build flash monitor
```
1. Flash bridge alone → cycling test patterns confirm the panel lights up.
2. Flash host + connect 4 wires → moving color bars confirm the full link.
3. Then feed `rgb_bridge_push_frame()` from your real source (LVGL framebuffer).

See `docs/06-rgb-bridge.md` (中文) for full pin tables and details.
