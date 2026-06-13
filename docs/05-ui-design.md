# Agent Indicator — 屏幕 UI 设计

> Rev 0.2 · English version: [en/05-ui-design.md](en/05-ui-design.md)
> 截图由 `tools/ui_sim`(LVGL PC 无头渲染)生成,与固件渲染像素一致。
> UI 代码位于 `firmware/main/ui/screens/`,固件与模拟器共用同一份源码。

**⚠ 圆形屏适配**:LCD 为 **480×480 圆形**显示区(AA 内的圆,四角不可见),
故 UI **不用 lv_tabview**(其全宽底栏与贴边卡片的四角会被圆裁掉)。改为:

- 内容限制在**居中安全区**(320×300,落在 R240 圆内),顶部留 66px 圆窄区;
- 导航为**底部居中 pill**(宽 290,落在底部弦内),6 图标切页;
- 截图均叠加圆形遮罩呈现真实可视区,圆外为面板外壳。

所有页面的数据更新接口(`ui_*_set`)内部带锁,可从任意任务调用;支持 Light/Dark 主题(§7)。

## 1. Home — 状态与 I/O 流

![Home](images/ui/ui-home.png)

- 顶栏:状态色点 + 状态名(跟随 i18n 语言)+ context 占用圆环(`128k`);
- 双用量条:会话(蓝)/ 5h 限额(橙),数据来自 USAGE 消息;
- 下方为 LLM 输入输出文本流(TEXT 消息,4KB 滚动窗口)。

## 2. Lighting — 灯效控制(参考 OpenRGB 预设)

![Lighting](images/ui/ui-light.png)

- 模式:**Agent**(默认状态驱动)/ **Solid** 常亮 / **Breath** 呼吸 /
  **Marquee** 跑马灯 / **Rainbow** 彩虹流动 / **Off**;
- RGB 三滑条 + 实时预览色块;Speed(1-100)与全局亮度;
- 等效控制途径:控制台 `light <mode> [RRGGBB] [speed]`、协议 CONFIG key=4/5;
- 覆盖灯效将 matrix+circle+bars 视为一条逻辑灯带统一渲染,限流器仍生效。

## 3. Wi-Fi — 连接管理

![Wi-Fi](images/ui/ui-wifi.png)

- 状态行(已连接 IP / disconnected)+ Scan 按钮(esp_wifi 异步扫描);
- 网络列表(RSSI + 加密标记),点击弹出全屏密码键盘,确认后
  `esp_wifi_set_config` 存 NVS 并重连。

## 4. Devices — 传感器与设备检测(= 设置页)

![Devices](images/ui/ui-devices.png)

自上而下四个区:

1. **环境读数卡**:温度 ℃ / 湿度 % / 气压 hPa(SHT4x + BMP280)+ 右侧 RTC 时钟,500ms 刷新;
2. **设备检测表**(可滚动,11 项):绿点=在位 / 红点=缺失,右列为 I2C 地址或状态。开机 `sensors_init` + `i2c_probe` 探测一次:

   | 设备 | 地址 | 说明 |
   |---|---|---|
   | TCA9554 IO expander | 0x20 | 慢速控制扩展 |
   | CST836U touch | 0x15 | 电容触摸 |
   | QMI8658C IMU | 0x6B | 6 轴 |
   | ES8311 codec | I2S | 音频 |
   | INA226 monitor | 0x40 | 功耗遥测 |
   | MP2760 charger | 0x5C | 充电管理 |
   | microSD | — | SDMMC 挂载状态 |
   | CAN bus | 500k | TWAI |
   | **SHT4x temp/humi** | **0x44** | **温湿度传感器** |
   | **BMP280 pressure** | **0x76** | **气压传感器(SDO=GND;0x77=SDO=VDD)** |
   | **PCF8563 RTC** | **0x51** | **I2C 实时时钟** |

3. **IMU 实时行**:6 轴加速度 + 温度,500ms 刷新;
4. **电源行 + 主题开关**:电压/电流/SoC,右侧 Light/Dark 开关(见 §7)。

新增三个 I2C 环境传感器驱动位于 `firmware/main/drivers/sensors.cpp`:SHT4x(0xFD 高精度测量)、
BMP280(读校准系数 + Bosch 定点补偿)、PCF8563(BCD 时间寄存器)。不在位时各 `*_present()`
返回 false,对应行显示红点。挂在共享 I2C 总线(Qwiic 座可直接接模块)。

## 5. Files — 文件浏览

![Files](images/ui/ui-files.png)

- 基于 LVGL FS(盘符 `A:` = POSIX VFS),可浏览 `/spiffs` 与 `/sdcard`;
- 目录点击进入,`..` 返回,路径栏循环滚动。

## 6. Music — 音乐播放

![Music](images/ui/ui-music.png)

- 曲目信息 + 进度条 + 播控(prev/play/next)+ 音量 arc(ES8311 DAC 音量);
- 后端为 `audio_player`(SD 卡 WAV);播放列表 glue 标注 TODO。

## 7. 主题 Light / Dark

全 UI 支持亮/暗双主题,Devices 页右下角开关切换,**NVS 持久化**(重启保留)。
实现:`screens.c` 维护两套调色板 token(`t_bg/t_card/t_text/t_sub/t_area`),
切换时重新 `lv_theme_default_init`(内置控件)+ 按 token 重建 UI,
并通过 `on_rebuild` 回调让宿主重新填充数据。也可由协议或控制台驱动。

| Dark(默认) | Light |
|---|---|
| ![Home dark](images/ui/ui-home.png) | ![Home light](images/ui/ui-light-home.png) |
| ![Devices dark](images/ui/ui-devices.png) | ![Devices light](images/ui/ui-light-devices.png) |

## 模拟器用法

```bash
cd tools/ui_sim
cmake -B build && cmake --build build -j   # 复用 firmware managed_components 里的 LVGL
./build/ui_sim shots/                      # 输出 ui-*.bmp(暗色)+ ui-light-*.bmp(亮色),共 12 张
```

注意:LVGL 必须配置为 CLIB malloc(固件 `CONFIG_LV_USE_CLIB_MALLOC`,
模拟器 `LV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB`)——内置 64KB TLSF 池容不下
六页控件,OOM 后会在 `lv_obj_add_style` 内死循环。
