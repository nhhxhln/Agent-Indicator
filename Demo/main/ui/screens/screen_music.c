/* Music 页:封面占位 + 曲目信息 + 进度 + 播控 + 音量。
 * 播放后端 = audio.cpp 的 audio_player(SD 卡 WAV),经 g_ui_api->music_cmd。 */
#include <stdio.h>

#include "screens_priv.h"

static lv_obj_t *s_title, *s_artist, *s_slider, *s_time_label, *s_play_label;
static int s_duration = 1;

static void btn_clicked(lv_event_t *e)
{
    int cmd = (int)(uintptr_t)lv_event_get_user_data(e);
    if (g_ui_api->music_cmd) g_ui_api->music_cmd(cmd);
}

static void vol_changed(lv_event_t *e)
{
    lv_obj_t *arc = lv_event_get_target(e);
    if (g_ui_api->music_volume) g_ui_api->music_volume(lv_arc_get_value(arc));
}

void ui_tab_music_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    /* 封面:圆形占位 + 音符符号 */
    lv_obj_t *art = lv_obj_create(parent);
    lv_obj_set_size(art, 76, 76);
    lv_obj_set_style_radius(art, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(art, lv_color_hex(0x1d2433), 0);
    lv_obj_set_style_border_color(art, lv_color_hex(0x3a4a6a), 0);
    lv_obj_set_style_border_width(art, 2, 0);
    lv_obj_t *note = lv_label_create(art);
    lv_label_set_text(note, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(note, &lv_font_montserrat_20, 0);
    lv_obj_center(note);

    s_title = lv_label_create(parent);
    lv_label_set_text(s_title, "--");
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_16, 0);
    s_artist = lv_label_create(parent);
    lv_label_set_text(s_artist, "--");
    lv_obj_set_style_text_color(s_artist, t_sub(), 0);

    /* 进度 */
    s_slider = lv_slider_create(parent);
    lv_obj_set_size(s_slider, LV_PCT(90), 8);
    s_time_label = lv_label_create(parent);
    lv_label_set_text(s_time_label, "0:00 / 0:00");
    lv_obj_set_style_text_color(s_time_label, t_sub(), 0);

    /* 播控行:prev / play / next + 音量 arc */
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 84);
    lv_obj_set_style_bg_opa(row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    const char *syms[3] = { LV_SYMBOL_PREV, LV_SYMBOL_PLAY, LV_SYMBOL_NEXT };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_button_create(row);
        lv_obj_set_size(btn, i == 1 ? 42 : 34, i == 1 ? 42 : 34);
        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, syms[i]);
        lv_obj_center(l);
        if (i == 1) s_play_label = l;
        lv_obj_add_event_cb(btn, btn_clicked, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);
    }

    lv_obj_t *vol = lv_arc_create(row);
    lv_obj_set_size(vol, 46, 46);
    lv_arc_set_range(vol, 0, 100);
    lv_arc_set_value(vol, 70);
    lv_obj_add_event_cb(vol, vol_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *vl = lv_label_create(vol);
    lv_label_set_text(vl, LV_SYMBOL_VOLUME_MAX);
    lv_obj_center(vl);
}

void ui_music_set_track(const char *title, const char *artist, int duration_s)
{
    ui_lock();
    if (!s_title) goto out;
    lv_label_set_text(s_title, title);
    lv_label_set_text(s_artist, artist);
    s_duration = duration_s > 0 ? duration_s : 1;
out:
    ui_unlock();
}

void ui_music_set_position(int pos_s, int playing)
{
    ui_lock();
    if (!s_slider) goto out;
    lv_slider_set_value(s_slider, pos_s * 100 / s_duration, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_time_label, "%d:%02d / %d:%02d",
                          pos_s / 60, pos_s % 60, s_duration / 60, s_duration % 60);
    if (s_play_label)
        lv_label_set_text(s_play_label, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
out:
    ui_unlock();
}
