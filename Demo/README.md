# Agent Indicator — Demo 测试工程

基于主固件裁剪,按测试用引脚配置:**LCD 换 GC9A01(SPI, 240×240 圆屏)**,
音频默认 **WM8960**,所有总线(SDMMC/I2C/I2S/TWAI/USB)同时启用。
目标板 **ESP32-S3-DevKitC-1(N16R8)** + 转接板。

## 引脚配置

| 外设 | 引脚 |
|---|---|
| LCD GC9A01(SPI) | CS=10, MOSI=11, SCLK=12, MISO=13, DC=14;RST 软复位,BL 接 VCC 常亮 |
| 音频 WM8960(I2S) | MCLK=1, BCLK=2, LRCK/WS=42, SDIN(ESP→codec)=41, DOUT(codec→ESP)=40 |
| I2C(触摸/codec/IMU/传感器) | SDA=39, SCL=38 |
| WS2812 ×2 | matrix=48, aux=45 |
| SDMMC(1 线) | CLK=4, CMD=5, DAT0=6 |
| TWAI(CAN) | TX=47, RX=21 |
| USB(USB-Serial-JTAG:日志/下载/REPL) | D-=19, D+=20 |
| 切页按钮 | BOOT(GPIO0) |

> SPI 屏省线后 21 根 GPIO 即可,全部互不冲突。SDMMC 未插卡只报错不 panic。
> WM8960 必须有 MCLK(已接 GPIO1);codec 默认探测 WM8960(0x1A),无则回落 ES8311(0x18)。

## 编译烧录

```bash
. /home/zhe/Code/esp-idf/export.sh
cd Demo
idf.py set-target esp32s3      # 首次
idf.py build flash monitor
```

## 测试方法

- **屏幕 UI**:开机显示六页圆形 UI。GC9A01 无触摸时 **按 BOOT 键轮播切页**
  (Home→Lighting→Wi-Fi→Devices→Files→Music);接了 CST816 触摸版则可触摸。
- **设备检测**:Devices 页(齿轮)看各外设绿/红点 + 环境读数(温湿度/气压/RTC)。
- **灯效**:Lighting 页切 Solid/Breath/Marquee/Rainbow;或控制台 `light rainbow`。
- **音频**:`tone 0`、`audio_loop on`、`audio_rec 5`+`audio_play`、
  `audio_rec_sd test 5`(需插卡)、`audio_player /sdcard/x.wav`、`audio_vol 60`。
- **CAN**:`can_tx 10 100` 发帧(PC `candump can0`);`can_rx on` 监听。
- **I2C 调试**:`i2cdetect` 扫描总线(确认 WM8960=0x1A 等是否在位);
  `i2cget <addr> <reg> [len]`、`i2cset <addr> <reg> <b0..>`(均 hex)。
- **SD**:`sd mount` / `ls /sdcard`。
- **IMU**:`imu` 单次,`imu_vis on` 可视化。
- **Wi-Fi**:PC 建 AP(SSID 见 menuconfig),设备自动连;host 跑 `agentind run`。
- **控制台**:USB 口 `idf.py monitor`,输入 `help` 看全部命令。

## 与主固件的差异

| | 主固件(firmware/) | Demo |
|---|---|---|
| LCD | ST7701 480×480 RGB(20 线) | **GC9A01 240×240 SPI(5 线)** |
| 音频默认 | ES8311 | **WM8960**(运行时仍兼容 ES8311) |
| SD/CAN | DevKitC-1 引脚不足默认禁用 | **默认启用**(SPI 屏腾出引脚) |
| 切页 | 触摸 | 触摸或 **BOOT 键** |

UI 源码与主固件共用(`main/ui/screens/`),仅安全区/字号按 240 圆屏缩放。
