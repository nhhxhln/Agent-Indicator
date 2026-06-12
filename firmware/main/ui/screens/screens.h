/* Agent Indicator UI 界面(纯 LVGL,无 esp 依赖)——
 * 同一份代码在固件(display.c 注入真实回调)与 PC 模拟器(tools/ui_sim
 * 注入 mock 数据)中编译,模拟器用于渲染文档截图。
 *
 * 页面:Home(状态+文本流)/ Wi-Fi / Devices(传感器与设备检测)
 *       / Files(文件浏览)/ Music(音乐播放)
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 宿主回调,NULL 安全(模拟器可全部不接)。
 * lock/unlock:所有 ui_*_set 公开函数内部自动调用,固件接 lvgl_port 锁,
 * 模拟器单线程留空。 */
typedef struct {
    void (*lock)(void);
    void (*unlock)(void);
    void (*wifi_scan)(void);
    void (*wifi_connect)(const char *ssid, const char *pass);
    void (*music_cmd)(int cmd);      /* 0 prev / 1 play-pause / 2 next */
    void (*music_volume)(int vol);   /* 0-100 */
    /* 灯效:mode 对应 led_fx_t(0 Agent/1 Solid/2 Breath/3 Marquee/4 Rainbow/5 Off) */
    void (*light_set)(int mode, uint8_t r, uint8_t g, uint8_t b,
                      int speed, int brightness);
} ui_host_api_t;

/* 创建整套 UI(挂在 active screen),返回 tabview 根对象 */
lv_obj_t *ui_screens_create(const ui_host_api_t *api);

/* ---- Home ---- */
void ui_home_set_state(const char *name, lv_color_t color);
void ui_home_append_text(const char *txt);
void ui_home_set_usage(int session_pct, int limit_pct);
void ui_home_set_context(int used_k, int total_k);

/* ---- Wi-Fi ---- */
void ui_wifi_set_status(const char *status);
void ui_wifi_clear_networks(void);
void ui_wifi_add_network(const char *ssid, int rssi, bool secured);

/* ---- Devices(行枚举与检测表对应) ---- */
typedef enum {
    UI_DEV_EXPANDER = 0,  /* TCA9554 */
    UI_DEV_TOUCH,         /* CST820 */
    UI_DEV_IMU,           /* QMI8658C */
    UI_DEV_CODEC,         /* ES8311 */
    UI_DEV_INA226,
    UI_DEV_CHARGER,       /* MP2760 */
    UI_DEV_SD,
    UI_DEV_CAN,
    UI_DEV_MAX,
} ui_dev_row_t;
void ui_devices_set(ui_dev_row_t row, bool present, const char *detail);
void ui_devices_set_imu_live(float ax, float ay, float az, float temp_c);
void ui_devices_set_power(int vbat_mv, int ibat_ma, int soc);

/* ---- Files ---- */
void ui_files_set_root(const char *lv_fs_path); /* 如 "A:/spiffs" */

/* ---- Music ---- */
void ui_music_set_track(const char *title, const char *artist, int duration_s);
void ui_music_set_position(int pos_s, int playing);

#ifdef __cplusplus
}
#endif
