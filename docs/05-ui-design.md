# Agent Indicator — 屏幕 UI 设计

> Rev 0.1 · English version: [en/05-ui-design.md](en/05-ui-design.md)
> 截图由 `tools/ui_sim`(LVGL PC 无头渲染)生成,与固件渲染像素一致。
> UI 代码位于 `firmware/main/ui/screens/`,固件与模拟器共用同一份源码。

整体结构:480×480 深色主题,底部 6 个 Tab(LVGL tabview),触摸切换。
所有页面的数据更新接口(`ui_*_set`)内部带锁,可从任意任务调用。

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

## 4. Devices — 传感器与设备检测

![Devices](images/ui/ui-devices.png)

- I2C/外设在位检测表(绿/红点 + 地址),开机探测一次;
- IMU 实时行(500ms 刷新)与电源遥测行(电压/电流/SoC)。

## 5. Files — 文件浏览

![Files](images/ui/ui-files.png)

- 基于 LVGL FS(盘符 `A:` = POSIX VFS),可浏览 `/spiffs` 与 `/sdcard`;
- 目录点击进入,`..` 返回,路径栏循环滚动。

## 6. Music — 音乐播放

![Music](images/ui/ui-music.png)

- 曲目信息 + 进度条 + 播控(prev/play/next)+ 音量 arc(ES8311 DAC 音量);
- 后端为 `audio_player`(SD 卡 WAV);播放列表 glue 标注 TODO。

## 模拟器用法

```bash
cd tools/ui_sim
cmake -B build && cmake --build build -j   # 复用 firmware managed_components 里的 LVGL
./build/ui_sim shots/                      # 输出 ui-*.bmp,六页各一张
```

注意:LVGL 必须配置为 CLIB malloc(固件 `CONFIG_LV_USE_CLIB_MALLOC`,
模拟器 `LV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB`)——内置 64KB TLSF 池容不下
六页控件,OOM 后会在 `lv_obj_add_style` 内死循环。
