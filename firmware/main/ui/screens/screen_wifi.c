/* Wi-Fi 页:状态卡 + 扫描按钮 + 网络列表;点击网络弹出密码键盘。 */
#include <stdio.h>
#include <string.h>

#include "screens_priv.h"

static lv_obj_t *s_status_label;
static lv_obj_t *s_list;
static lv_obj_t *s_kb_cont, *s_kb_ta;
static char s_pending_ssid[33];

static void connect_clicked(lv_event_t *e)
{
    (void)e;
    if (g_ui_api->wifi_connect)
        g_ui_api->wifi_connect(s_pending_ssid, lv_textarea_get_text(s_kb_ta));
    lv_obj_add_flag(s_kb_cont, LV_OBJ_FLAG_HIDDEN);
}

static void kb_event(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_READY) connect_clicked(e);
    if (lv_event_get_code(e) == LV_EVENT_CANCEL)
        lv_obj_add_flag(s_kb_cont, LV_OBJ_FLAG_HIDDEN);
}

static void password_dialog_open(const char *ssid)
{
    strncpy(s_pending_ssid, ssid, sizeof(s_pending_ssid) - 1);
    lv_textarea_set_text(s_kb_ta, "");
    lv_textarea_set_placeholder_text(s_kb_ta, ssid);
    lv_obj_remove_flag(s_kb_cont, LV_OBJ_FLAG_HIDDEN);
}

static void net_clicked(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *txt = lv_list_get_button_text(s_list, btn);
    /* 列表文本形如 "ssid  (-52dBm)",截取 ssid */
    char ssid[33] = { 0 };
    sscanf(txt, "%32s", ssid);
    password_dialog_open(ssid);
}

static void scan_clicked(lv_event_t *e)
{
    (void)e;
    if (g_ui_api->wifi_scan) g_ui_api->wifi_scan();
}

void ui_tab_wifi_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);

    lv_obj_t *top = ui_card(parent);
    lv_obj_set_size(top, LV_PCT(100), 70);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    s_status_label = lv_label_create(top);
    lv_label_set_text(s_status_label, LV_SYMBOL_WIFI " --");
    lv_obj_t *btn = lv_button_create(top);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, LV_SYMBOL_REFRESH " Scan");
    lv_obj_add_event_cb(btn, scan_clicked, LV_EVENT_CLICKED, NULL);

    s_list = lv_list_create(parent);
    lv_obj_set_width(s_list, LV_PCT(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(0x101018), 0);

    /* 密码输入浮层(置于顶层,默认隐藏) */
    s_kb_cont = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_kb_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_kb_cont, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_kb_cont, LV_OPA_70, 0);
    lv_obj_add_flag(s_kb_cont, LV_OBJ_FLAG_HIDDEN);
    s_kb_ta = lv_textarea_create(s_kb_cont);
    lv_obj_set_size(s_kb_ta, LV_PCT(90), 44);
    lv_obj_align(s_kb_ta, LV_ALIGN_TOP_MID, 0, 60);
    lv_textarea_set_one_line(s_kb_ta, true);
    lv_textarea_set_password_mode(s_kb_ta, true);
    lv_obj_t *kb = lv_keyboard_create(s_kb_cont);
    lv_keyboard_set_textarea(kb, s_kb_ta);
    lv_obj_add_event_cb(kb, kb_event, LV_EVENT_ALL, NULL);
}

void ui_wifi_set_status(const char *status)
{
    ui_lock();
    if (s_status_label)
        lv_label_set_text_fmt(s_status_label, LV_SYMBOL_WIFI " %s", status);
    ui_unlock();
}

void ui_wifi_clear_networks(void)
{
    ui_lock();
    if (s_list) lv_obj_clean(s_list);
    ui_unlock();
}

void ui_wifi_add_network(const char *ssid, int rssi, bool secured)
{
    ui_lock();
    if (!s_list) goto out;
    char txt[64];
    snprintf(txt, sizeof(txt), "%s  (%ddBm)%s", ssid, rssi,
             secured ? "  " LV_SYMBOL_EYE_CLOSE : "");
    lv_obj_t *btn = lv_list_add_button(s_list, LV_SYMBOL_WIFI, txt);
    lv_obj_add_event_cb(btn, net_clicked, LV_EVENT_CLICKED, NULL);
out:
    ui_unlock();
}
