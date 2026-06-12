# Agent Indicator — On-screen UI Design

> Rev 0.1 · 中文版: [../05-ui-design.md](../05-ui-design.md)
> Screenshots are produced by `tools/ui_sim` (headless LVGL on PC) and are
> pixel-identical to the firmware rendering — both compile the same sources
> in `firmware/main/ui/screens/`.

Structure: 480×480 dark theme, 6 bottom tabs (LVGL tabview), touch to switch.
All data-update entry points (`ui_*_set`) are internally locked and callable
from any task.

## 1. Home — status & I/O stream

![Home](../images/ui/ui-home.png)

- Top bar: state color dot + state name (follows i18n language) + context arc (`128k`);
- Two usage bars: session (blue) / 5h limit (orange), fed by USAGE messages;
- Below: the LLM input/output text stream (TEXT messages, 4KB scrollback).

## 2. Lighting — LED effects (OpenRGB-style presets)

![Lighting](../images/ui/ui-light.png)

- Modes: **Agent** (default state-driven) / **Solid** / **Breath** /
  **Marquee** / **Rainbow** / **Off**;
- RGB sliders + live preview swatch; Speed (1-100) and global brightness;
- Equivalent controls: console `light <mode> [RRGGBB] [speed]`, protocol CONFIG key=4/5;
- Override effects treat matrix+circle+bars as one logical strip; the current
  limiter still applies.

## 3. Wi-Fi — connection management

![Wi-Fi](../images/ui/ui-wifi.png)

- Status row (connected IP / disconnected) + Scan button (async esp_wifi scan);
- Network list (RSSI + lock mark); tapping pops a full-screen password keyboard,
  then `esp_wifi_set_config` persists to NVS and reconnects.

## 4. Devices — sensor & peripheral detection

![Devices](../images/ui/ui-devices.png)

- Presence table (green/red dot + address) probed at boot;
- Live IMU row (500ms refresh) and power telemetry row (V/I/SoC).

## 5. Files — file browser

![Files](../images/ui/ui-files.png)

- Backed by LVGL FS (drive `A:` = POSIX VFS); browses `/spiffs` and `/sdcard`;
- Tap to enter directories, `..` to go up, circular-scrolling path bar.

## 6. Music — player

![Music](../images/ui/ui-music.png)

- Track info + progress + transport (prev/play/next) + volume arc (ES8311 DAC);
- Backend is `audio_player` (WAV from SD); playlist glue marked TODO.

## Simulator usage

```bash
cd tools/ui_sim
cmake -B build && cmake --build build -j   # reuses LVGL from firmware managed_components
./build/ui_sim shots/                      # writes ui-*.bmp, one per tab
```

Note: LVGL must use CLIB malloc (`CONFIG_LV_USE_CLIB_MALLOC` in firmware,
`LV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB` in the simulator) — the built-in 64KB
TLSF pool can't hold six pages of widgets and OOM ends in an infinite loop
inside `lv_obj_add_style`.
