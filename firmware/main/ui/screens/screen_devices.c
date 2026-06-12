/* Devices 页:I2C/外设检测表 + IMU 实时值 + 电源遥测。 */
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
};

static lv_obj_t *s_dot[UI_DEV_MAX];
static lv_obj_t *s_detail[UI_DEV_MAX];
static lv_obj_t *s_imu_label, *s_power_label;

void ui_tab_devices_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);

    lv_obj_t *card = ui_card(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 4, 0);

    for (int i = 0; i < UI_DEV_MAX; i++) {
        lv_obj_t *row = lv_obj_create(card);
        lv_obj_set_size(row, LV_PCT(100), 32);
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
        lv_obj_set_style_pad_left(name, 8, 0);
        lv_obj_set_flex_grow(name, 1);

        s_detail[i] = lv_label_create(row);
        lv_label_set_text(s_detail[i], "--");
        lv_obj_set_style_text_color(s_detail[i], lv_color_hex(0x8888aa), 0);
    }

    lv_obj_t *imu_card = ui_card(parent);
    lv_obj_set_size(imu_card, LV_PCT(100), 56);
    s_imu_label = lv_label_create(imu_card);
    lv_label_set_text(s_imu_label, "IMU: --");

    lv_obj_t *pwr_card = ui_card(parent);
    lv_obj_set_size(pwr_card, LV_PCT(100), 56);
    s_power_label = lv_label_create(pwr_card);
    lv_label_set_text(s_power_label, LV_SYMBOL_BATTERY_2 " --");
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
            LV_SYMBOL_BATTERY_2 " %d.%03dV  %dmA  SoC %d%%",
            vbat_mv / 1000, vbat_mv % 1000, ibat_ma, soc);
    ui_unlock();
}
