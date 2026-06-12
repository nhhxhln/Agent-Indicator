# Agent Indicator

[中文](README.md) | English

An LLM status indicator desk gadget: RGB Matrix (context) · RGB Circle (status) ·
RGB Bars (usage / mic VU) · LCD (I/O text), driven by an ESP32-S3 with three host
links (Wi-Fi / CAN / USB), powered by 3×18650 + USB-PD/XT30.

![Console](docs/images/console-3d.png)

```
docs/en/                   Design docs (system / power / industrial / schematics)
docs/                      中文文档
hardware/3d/               OpenSCAD models + printable STL
host/                      Python bridge "agentind" (sources → protocol → ws/can/usb)
firmware/                  ESP-IDF v5.2 firmware (LVGL9, audio, CAN, IMU, test console)
```

Quick start: see `host/README.en.md` and `firmware/README.en.md`.
Hardware route: build the Console (variant B) first; the main board is shared by all
four enclosure variants.
