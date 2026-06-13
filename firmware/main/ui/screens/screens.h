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
    void (*theme_persist)(int dark); /* UI 切换主题后写 NVS(可空) */
    void (*on_rebuild)(void);        /* 主题切换重建 UI 后回调,宿主重新填数据(可空) */
} ui_host_api_t;

/* 创建整套 UI(挂在 active screen),dark=1 暗色/0 亮色,返回 tabview 根对象 */
lv_obj_t *ui_screens_create(const ui_host_api_t *api, int dark);
/* 运行时切换主题(重建 UI,触发 on_rebuild) */
void ui_screens_set_theme(int dark);
int  ui_screens_is_dark(void);
/* 在 create 前设置 UI 字体(如 TTF);NULL = 内置默认 */
void ui_set_font(const lv_font_t *font);
/* 切换到第 idx 页(0 Home/1 Lighting/2 Wi-Fi/3 Devices/4 Files/5 Music) */
void ui_screens_goto(int idx);

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
    UI_DEV_TOUCH,         /* CST836U */
    UI_DEV_IMU,           /* QMI8658C */
    UI_DEV_CODEC,         /* ES8311 */
    UI_DEV_INA226,
    UI_DEV_CHARGER,       /* MP2760 */
    UI_DEV_SD,
    UI_DEV_CAN,
    UI_DEV_SHT,           /* SHT4x 温湿度 */
    UI_DEV_BMP,           /* BMP280 气压 */
    UI_DEV_RTC,           /* PCF8563 RTC */
    UI_DEV_MAX,
} ui_dev_row_t;
void ui_devices_set(ui_dev_row_t row, bool present, const char *detail);
void ui_devices_set_imu_live(float ax, float ay, float az, float temp_c);
void ui_devices_set_power(int vbat_mv, int ibat_ma, int soc);
/* 环境读数卡:温度℃ / 湿度% / 气压 hPa / 时钟字符串 */
void ui_devices_set_env(float temp_c, float humi_pct, float press_hpa);
void ui_devices_set_time(const char *clock_str);

/* ---- Files ---- */
void ui_files_set_root(const char *lv_fs_path); /* 如 "A:/spiffs" */

/* ---- Music ---- */
void ui_music_set_track(const char *title, const char *artist, int duration_s);
void ui_music_set_position(int pos_s, int playing);

#ifdef __cplusplus
}
#endif
