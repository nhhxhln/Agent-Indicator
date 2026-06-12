# Agent Indicator Firmware (ESP-IDF v5.2)

[中文](README.md) | English

## Build & Flash

```bash
. /home/zhe/Code/esp-idf/export.sh
idf.py set-target esp32s3
idf.py menuconfig        # "Agent Indicator" menu: Wi-Fi SSID/password, matrix tiles, VLED budget
idf.py build flash monitor
```

## Layout

```
main/
  board.h            pinmap (matches docs/en/04 §2 — board changes happen here only)
  app_main.c         boot order
  app_state.{h,c}    global state model: comm writes, ui/audio consume; text ring buffer
  proto/             protocol frames (mirrors host/agentind/protocol.py)
  bus/i2c_bus.cpp    shared I2C bus + TCA9554 expander (no-op fallback when absent)
  storage/           SPIFFS (/spiffs) + microSD 1-bit (/sdcard)
  console/           esp_console REPL @ USB Serial/JTAG, case registration hub
  comm/              comm_wifi (WS server + mDNS) / comm_twai (0x500/0x580 segments)
                     / comm_usb (TinyUSB vendor skeleton; exclusive with the REPL)
  ui/                led_engine (60fps, current limiter, 1 or 4 matrix tiles)
                     display (ST7701 3-wire-init + LVGL9 + CST820 + TinyTTF)
                     i18n (zh/en string tables, NVS-persisted)
  audio/audio.cpp    ES8311 full-duplex 16k/16bit; one capture task feeds VU/rec/loopback
  drivers/qmi8658.cpp QMI8658C minimal driver (±8g/±512dps)
  cases/             CAN / IMU test cases (audio cases live in audio.cpp)
  power/power.c      telemetry task (INA226/MP2760/cell ADC pending bring-up)
```

## Test Console (USB Serial/JTAG, available in `idf.py monitor`)

| Command | Function |
|---|---|
| `can_tx [count] [period_ms]` | send incrementing frames (ID 0x123); pair with `candump can0` |
| `can_rx [on\|off]` | bus monitor (turn off while testing protocol frames — competes with comm_twai) |
| `can_status` | TWAI state / error counters |
| `audio_rec [sec]` | record to PSRAM (≤30s) |
| `audio_play` | replay the PSRAM recording |
| `audio_rec_sd <name> [sec]` | record to `/sdcard/<name>.wav` |
| `audio_player <path>` | play a WAV (`/sdcard` or `/spiffs`, PCM16) |
| `audio_loop [on\|off]` | MIC→SPK passthrough |
| `audio_vol <0-100>` / `tone <0-3>` | volume / chimes |
| `imu` / `imu_vis [on\|off]` | one-shot read / live view (LCD horizon + 6-axis bars, logs without LCD) |
| `sd [mount\|umount]` / `ls [path]` | SD management / list dir |
| `lang [zh\|en]` | UI language (persisted in NVS) |

## Task Stacks (C++ module notes)

| Task | Stack | Notes |
|---|---|---|
| LVGL (esp_lvgl_port) | 10240 | TinyTTF rasterization runs here |
| console REPL | 8192 | commands run on this task (audio_play blocks until done) |
| audio player | 8192 | vfs + WAV parsing |
| audio capture | 6144 | file-write path |
| imu_vis / disp_text | 6144 | LVGL calls + float printf |
| can_tx/can_rx | 4096 | driver + log only |

New code is C++17 with `extern "C"` across the C boundary; task entry points must not
throw (exceptions disabled); no large buffers on stacks (>512B goes static or heap).

## Bridge bring-up (works without any peripherals)

```bash
# PC side (AP already up)
cd ../host && pip install -e . && agentind run -s demo -v
```

All hardware dependencies (LCD/audio/IMU/expander) are runtime-probed; a bare
ESP32-S3 module is enough to exercise Wi-Fi + protocol + state machine, with the
text stream degraded to monitor logs.
