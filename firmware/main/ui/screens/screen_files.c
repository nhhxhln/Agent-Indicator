/* Files 页:LVGL FS('A:' = POSIX VFS)文件浏览,目录点击进入,".." 返回。 */
#include <stdio.h>
#include <string.h>

#include "screens_priv.h"

static lv_obj_t *s_path_label;
static lv_obj_t *s_list;
static char s_path[160] = "A:/";

static void refresh(void);

static void entry_clicked(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *name = lv_list_get_button_text(s_list, btn);
    bool is_dir = (bool)(uintptr_t)lv_event_get_user_data(e);
    if (strcmp(name, "..") == 0) {
        char *slash = strrchr(s_path, '/');
        if (slash && slash > strchr(s_path, '/')) *slash = '\0';
        else snprintf(s_path, sizeof(s_path), "A:/");
        refresh();
    } else if (is_dir) {
        size_t len = strlen(s_path);
        snprintf(s_path + len, sizeof(s_path) - len, "%s%s",
                 s_path[len - 1] == '/' ? "" : "/", name);
        refresh();
    }
}

static void add_entry(const char *name, bool is_dir)
{
    lv_obj_t *btn = lv_list_add_button(
        s_list, is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE, name);
    lv_obj_add_event_cb(btn, entry_clicked, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)is_dir);
}

static void refresh(void)
{
    lv_label_set_text(s_path_label, s_path);
    lv_obj_clean(s_list);
    add_entry("..", true);

    lv_fs_dir_t dir;
    if (lv_fs_dir_open(&dir, s_path) != LV_FS_RES_OK) {
        lv_list_add_text(s_list, "(open failed)");
        return;
    }
    char name[128];
    int count = 0;
    while (lv_fs_dir_read(&dir, name, sizeof(name)) == LV_FS_RES_OK &&
           name[0] != '\0' && count < 64) {
        bool is_dir = name[0] == '/';
        add_entry(is_dir ? name + 1 : name, is_dir);
        count++;
    }
    lv_fs_dir_close(&dir);
    if (count == 0) lv_list_add_text(s_list, "(empty)");
}

static void refresh_clicked(lv_event_t *e)
{
    (void)e;
    refresh();
}

void ui_tab_files_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 8, 0);

    lv_obj_t *top = ui_card(parent);
    lv_obj_set_size(top, LV_PCT(100), 56);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    s_path_label = lv_label_create(top);
    lv_label_set_text(s_path_label, s_path);
    lv_label_set_long_mode(s_path_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_flex_grow(s_path_label, 1);
    lv_obj_t *btn = lv_button_create(top);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, LV_SYMBOL_REFRESH);
    lv_obj_add_event_cb(btn, refresh_clicked, LV_EVENT_CLICKED, NULL);

    s_list = lv_list_create(parent);
    lv_obj_set_width(s_list, LV_PCT(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(0x101018), 0);
}

void ui_files_set_root(const char *lv_fs_path)
{
    ui_lock();
    snprintf(s_path, sizeof(s_path), "%s", lv_fs_path);
    if (s_list) refresh();
    ui_unlock();
}
