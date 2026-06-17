/* Lighting 页:灯效控制(参考 OpenRGB 预设)。
 * 模式:Agent(状态驱动)/ Solid / Breath / Marquee / Rainbow / Off
 * 颜色 RGB 三滑条 + 实时预览,速度与全局亮度滑条。
 * 所有变更经 g_ui_api->light_set 下发到 led_engine。 */
#include <stdio.h>

#include "screens_priv.h"

static const char *MODE_MAP[] = { "Agent", "Solid", "Breath", "\n",
                                  "Marquee", "Rainbow", "Off", "" };

static lv_obj_t *s_modes, *s_preview;
static lv_obj_t *s_slider_rgb[3], *s_label_rgb[3];
static lv_obj_t *s_slider_speed, *s_slider_bright;
static int s_mode = 0;

static void push_state(void)
{
    int r = lv_slider_get_value(s_slider_rgb[0]);
    int g = lv_slider_get_value(s_slider_rgb[1]);
    int b = lv_slider_get_value(s_slider_rgb[2]);
    lv_obj_set_style_bg_color(s_preview, lv_color_make(r, g, b), 0);
    for (int i = 0; i < 3; i++)
        lv_label_set_text_fmt(s_label_rgb[i], "%d",
                              (int)lv_slider_get_value(s_slider_rgb[i]));
    if (g_ui_api->light_set)
        g_ui_api->light_set(s_mode, r, g, b,
                            lv_slider_get_value(s_slider_speed),
                            lv_slider_get_value(s_slider_bright));
}

static void mode_changed(lv_event_t *e)
{
    lv_obj_t *bm = lv_event_get_target(e);
    s_mode = (int)lv_buttonmatrix_get_selected_button(bm);
    push_state();
}

static void slider_changed(lv_event_t *e)
{
    (void)e;
    push_state();
}

static lv_obj_t *labeled_slider(lv_obj_t *parent, const char *name,
                                int min, int max, int val, lv_color_t color)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 36);
    lv_obj_set_style_bg_opa(row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(row);
    lv_label_set_text(l, name);
    lv_obj_set_width(l, 64);

    lv_obj_t *s = lv_slider_create(row);
    lv_slider_set_range(s, min, max);
    lv_slider_set_value(s, val, LV_ANIM_OFF);
    lv_obj_set_height(s, 8);
    lv_obj_set_flex_grow(s, 1);
    lv_obj_set_style_bg_color(s, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, color, LV_PART_KNOB);
    lv_obj_add_event_cb(s, slider_changed, LV_EVENT_VALUE_CHANGED, NULL);
    return s;
}

void ui_tab_light_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);

    /* 模式选择 */
    s_modes = lv_buttonmatrix_create(parent);
    lv_buttonmatrix_set_map(s_modes, MODE_MAP);
    lv_buttonmatrix_set_one_checked(s_modes, true);
    lv_buttonmatrix_set_button_ctrl_all(s_modes, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_buttonmatrix_set_button_ctrl(s_modes, 0, LV_BUTTONMATRIX_CTRL_CHECKED);
    lv_obj_set_size(s_modes, LV_PCT(100), 104);
    lv_obj_set_style_bg_color(s_modes, t_card(), 0);
    lv_obj_add_event_cb(s_modes, mode_changed, LV_EVENT_VALUE_CHANGED, NULL);

    /* 颜色卡:预览 + RGB 滑条 */
    lv_obj_t *card = ui_card(parent);
    lv_obj_set_size(card, LV_PCT(100), 176);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 4, 0);

    s_preview = lv_obj_create(card);
    lv_obj_set_size(s_preview, LV_PCT(100), 28);
    lv_obj_set_style_radius(s_preview, 6, 0);
    lv_obj_set_style_bg_color(s_preview, lv_color_hex(0x008cff), 0);
    lv_obj_set_style_border_width(s_preview, 0, 0);

    static const char *names[3] = { "R", "G", "B" };
    static const uint32_t colors[3] = { 0xcc3344, 0x33cc66, 0x3366ee };
    static const int defs[3] = { 0, 140, 255 };
    for (int i = 0; i < 3; i++) {
        s_slider_rgb[i] = labeled_slider(card, names[i], 0, 255, defs[i],
                                         lv_color_hex(colors[i]));
        /* 行尾数值 */
        lv_obj_t *row = lv_obj_get_parent(s_slider_rgb[i]);
        s_label_rgb[i] = lv_label_create(row);
        lv_label_set_text_fmt(s_label_rgb[i], "%d", defs[i]);
        lv_obj_set_width(s_label_rgb[i], 40);
        lv_obj_set_style_text_align(s_label_rgb[i], LV_TEXT_ALIGN_RIGHT, 0);
    }

    /* 速度 / 亮度 */
    lv_obj_t *card2 = ui_card(parent);
    lv_obj_set_width(card2, LV_PCT(100));
    lv_obj_set_flex_grow(card2, 1);
    lv_obj_set_flex_flow(card2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card2, 4, 0);
    s_slider_speed = labeled_slider(card2, "Speed", 1, 100, 40,
                                    lv_color_hex(0x8888aa));
    s_slider_bright = labeled_slider(card2, "Bright", 5, 100, 63,
                                     lv_color_hex(0xccaa33));
}
