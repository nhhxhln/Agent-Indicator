/* Devices 页(= 设置页):
 *   顶部:环境读数卡(温度/湿度/气压 + RTC 时钟)
 *   中部:I2C/外设在位检测表(可滚动)
 *   底部:IMU 实时行 + 电源遥测行 + 主题(Light/Dark)开关 */
#include <stdio.h>

#include "screens_priv.h"

static const char *DEV_NAMES[UI_DEV_MAX] = {
    "TCA9554 IO expander",
    "CST820 touch",
    "QMI8658C IMU",
    "ES8311 codec",
    "INA226 monitor",
    "MP2760 charger",
    "microSD",
    "CAN bus",
    "SHT4x temp/humi",
    "BMP280 pressure",
    "PCF8563 RTC",
};

static lv_obj_t *s_dot[UI_DEV_MAX];
static lv_obj_t *s_detail[UI_DEV_MAX];
static lv_obj_t *s_imu_label, *s_power_label;
static lv_obj_t *s_env_label, *s_clock_label;
static lv_obj_t *s_theme_sw;

static void theme_sw_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    /* checked = Dark;重建后控件失效,事件回调在重建前读值即可 */
    ui_screens_set_theme(lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0);
}

void ui_tab_devices_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);

    /* 环境读数卡(温湿度/气压 + 时钟) */
    lv_obj_t *env = ui_card(parent);
    lv_obj_set_size(env, LV_PCT(100), 60);
    lv_obj_set_flex_flow(env, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(env, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    s_env_label = lv_label_create(env);
    lv_label_set_text(s_env_label, "-- C   -- %   -- hPa");
    lv_obj_set_style_text_color(s_env_label, t_text(), 0);
    s_clock_label = lv_label_create(env);
    lv_label_set_text(s_clock_label, "--:--");
    lv_obj_set_style_text_color(s_clock_label, t_sub(), 0);

    /* 检测表(可滚动) */
    lv_obj_t *card = ui_card(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 4, 0);

    for (int i = 0; i < UI_DEV_MAX; i++) {
        lv_obj_t *row = lv_obj_create(card);
        lv_obj_set_size(row, LV_PCT(100), 30);
        lv_obj_set_style_bg_opa(row, LV_OPA_0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 2, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        s_dot[i] = lv_obj_create(row);
        lv_obj_set_size(s_dot[i], 12, 12);
        lv_obj_set_style_radius(s_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_dot[i], lv_color_hex(0x555555), 0);
        lv_obj_set_style_border_width(s_dot[i], 0, 0);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, DEV_NAMES[i]);
        lv_obj_set_style_text_color(name, t_text(), 0);
        lv_obj_set_style_pad_left(name, 8, 0);
        lv_obj_set_flex_grow(name, 1);

        s_detail[i] = lv_label_create(row);
        lv_label_set_text(s_detail[i], "--");
        lv_obj_set_style_text_color(s_detail[i], t_sub(), 0);
    }

    lv_obj_t *imu_card = ui_card(parent);
    lv_obj_set_size(imu_card, LV_PCT(100), 48);
    s_imu_label = lv_label_create(imu_card);
    lv_label_set_text(s_imu_label, "IMU: --");
    lv_obj_set_style_text_color(s_imu_label, t_text(), 0);

    lv_obj_t *pwr_card = ui_card(parent);
    lv_obj_set_size(pwr_card, LV_PCT(100), 48);
    lv_obj_set_flex_flow(pwr_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pwr_card, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_power_label = lv_label_create(pwr_card);
    lv_label_set_text(s_power_label, LV_SYMBOL_BATTERY_2 " --");
    lv_obj_set_style_text_color(s_power_label, t_text(), 0);
    /* 主题开关(checked = Dark) */
    lv_obj_t *box = lv_obj_create(pwr_card);
    lv_obj_set_size(box, 130, 36);
    lv_obj_set_style_bg_opa(box, LV_OPA_0, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *tl = lv_label_create(box);
    lv_label_set_text(tl, LV_SYMBOL_TINT " Dark");
    lv_obj_set_style_text_color(tl, t_sub(), 0);
    lv_obj_set_style_pad_right(tl, 6, 0);
    s_theme_sw = lv_switch_create(box);
    if (g_ui_dark) lv_obj_add_state(s_theme_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_theme_sw, theme_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

void ui_devices_set(ui_dev_row_t row, bool present, const char *detail)
{
    ui_lock();
    if (row >= UI_DEV_MAX || !s_dot[row]) goto out;
    lv_obj_set_style_bg_color(s_dot[row],
        present ? lv_color_hex(0x22cc66) : lv_color_hex(0xcc3333), 0);
    if (detail) lv_label_set_text(s_detail[row], detail);
out:
    ui_unlock();
}

void ui_devices_set_imu_live(float ax, float ay, float az, float temp_c)
{
    ui_lock();
    if (s_imu_label)
        lv_label_set_text_fmt(s_imu_label,
            "IMU: %+.2f %+.2f %+.2f g   %.1f C",
            (double)ax, (double)ay, (double)az, (double)temp_c);
    ui_unlock();
}

void ui_devices_set_power(int vbat_mv, int ibat_ma, int soc)
{
    ui_lock();
    if (s_power_label)
        lv_label_set_text_fmt(s_power_label,
            LV_SYMBOL_BATTERY_2 " %d.%03dV %dmA %d%%",
            vbat_mv / 1000, vbat_mv % 1000, ibat_ma, soc);
    ui_unlock();
}

void ui_devices_set_env(float temp_c, float humi_pct, float press_hpa)
{
    ui_lock();
    if (s_env_label)
        lv_label_set_text_fmt(s_env_label, "%.1f C   %.0f %%   %.0f hPa",
                              (double)temp_c, (double)humi_pct, (double)press_hpa);
    ui_unlock();
}

void ui_devices_set_time(const char *clock_str)
{
    ui_lock();
    if (s_clock_label && clock_str) lv_label_set_text(s_clock_label, clock_str);
    ui_unlock();
}
