#include "screens_priv.h"

#include <stdio.h>
#include <string.h>

static const ui_host_api_t s_null_api; /* 全 NULL,调用前判空 */
const ui_host_api_t *g_ui_api = &s_null_api;

/* ---- Home 控件 ---- */
static lv_obj_t *s_state_dot, *s_state_label;
static lv_obj_t *s_textarea;
static lv_obj_t *s_bar_session, *s_bar_limit, *s_ctx_arc, *s_ctx_label;

lv_obj_t *ui_card(lv_obj_t *parent)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_style_bg_color(c, lv_color_hex(0x16161f), 0);
    lv_obj_set_style_border_color(c, lv_color_hex(0x2a2a38), 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 10, 0);
    lv_obj_set_style_pad_all(c, 10, 0);
    return c;
}

void ui_tab_home_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);

    /* 顶栏:状态点 + 状态名 + context 圆环 */
    lv_obj_t *top = ui_card(parent);
    lv_obj_set_size(top, LV_PCT(100), 86);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    s_state_dot = lv_obj_create(top);
    lv_obj_set_size(s_state_dot, 22, 22);
    lv_obj_set_style_radius(s_state_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_state_dot, lv_color_hex(0x444455), 0);
    lv_obj_set_style_border_width(s_state_dot, 0, 0);

    s_state_label = lv_label_create(top);
    lv_label_set_text(s_state_label, "Offline");
    lv_obj_set_style_text_font(s_state_label, &lv_font_montserrat_24, 0);
    lv_obj_set_flex_grow(s_state_label, 1);
    lv_obj_set_style_pad_left(s_state_label, 10, 0);

    s_ctx_arc = lv_arc_create(top);
    lv_obj_set_size(s_ctx_arc, 60, 60);
    lv_arc_set_rotation(s_ctx_arc, 270);
    lv_arc_set_bg_angles(s_ctx_arc, 0, 360);
    lv_arc_set_value(s_ctx_arc, 0);
    lv_obj_remove_style(s_ctx_arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(s_ctx_arc, LV_OBJ_FLAG_CLICKABLE);
    s_ctx_label = lv_label_create(s_ctx_arc);
    lv_label_set_text(s_ctx_label, "ctx");
    lv_obj_set_style_text_font(s_ctx_label, &lv_font_montserrat_14, 0);
    lv_obj_center(s_ctx_label);

    /* 用量条 ×2 */
    lv_obj_t *usage = ui_card(parent);
    lv_obj_set_size(usage, LV_PCT(100), 64);
    lv_obj_set_flex_flow(usage, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(usage, 8, 0);
    s_bar_session = lv_bar_create(usage);
    lv_obj_set_size(s_bar_session, LV_PCT(100), 12);
    s_bar_limit = lv_bar_create(usage);
    lv_obj_set_size(s_bar_limit, LV_PCT(100), 12);
    lv_obj_set_style_bg_color(s_bar_limit, lv_color_hex(0x223), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar_limit, lv_color_hex(0xcc8800), LV_PART_INDICATOR);

    /* I/O 文本流 */
    s_textarea = lv_textarea_create(parent);
    lv_obj_set_width(s_textarea, LV_PCT(100));
    lv_obj_set_flex_grow(s_textarea, 1);
    lv_obj_set_style_bg_color(s_textarea, lv_color_hex(0x101018), 0);
    lv_obj_set_style_text_color(s_textarea, lv_color_hex(0xd0d0e0), 0);
    lv_textarea_set_max_length(s_textarea, 4096);
}

void ui_home_set_state(const char *name, lv_color_t color)
{
    ui_lock();
    if (!s_state_label) goto out;
    lv_label_set_text(s_state_label, name);
    lv_obj_set_style_bg_color(s_state_dot, color, 0);
out:
    ui_unlock();
}

void ui_home_append_text(const char *txt)
{
    ui_lock();
    if (s_textarea) lv_textarea_add_text(s_textarea, txt);
    ui_unlock();
}

void ui_home_set_usage(int session_pct, int limit_pct)
{
    ui_lock();
    if (!s_bar_session) goto out;
    if (session_pct >= 0) lv_bar_set_value(s_bar_session, session_pct, LV_ANIM_ON);
    if (limit_pct >= 0) lv_bar_set_value(s_bar_limit, limit_pct, LV_ANIM_ON);
out:
    ui_unlock();
}

void ui_home_set_context(int used_k, int total_k)
{
    ui_lock();
    if (!s_ctx_arc || total_k <= 0) goto out;
    lv_arc_set_value(s_ctx_arc, used_k * 100 / total_k);
    lv_label_set_text_fmt(s_ctx_label, "%dk", used_k);
out:
    ui_unlock();
}

/* ---- 根:tabview ---- */
lv_obj_t *ui_screens_create(const ui_host_api_t *api)
{
    if (api) g_ui_api = api;

    lv_obj_t *tv = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_position(tv, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(tv, 52);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0a0a12), 0);

    ui_tab_home_create(lv_tabview_add_tab(tv, LV_SYMBOL_HOME));
    ui_tab_light_create(lv_tabview_add_tab(tv, LV_SYMBOL_TINT));
    ui_tab_wifi_create(lv_tabview_add_tab(tv, LV_SYMBOL_WIFI));
    ui_tab_devices_create(lv_tabview_add_tab(tv, LV_SYMBOL_SETTINGS));
    ui_tab_files_create(lv_tabview_add_tab(tv, LV_SYMBOL_DIRECTORY));
    ui_tab_music_create(lv_tabview_add_tab(tv, LV_SYMBOL_AUDIO));
    return tv;
}
