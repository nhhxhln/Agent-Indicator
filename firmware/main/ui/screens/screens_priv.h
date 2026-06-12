/* screens 内部共享 */
#pragma once

#include "screens.h"

extern const ui_host_api_t *g_ui_api; /* 永不为 NULL(默认空实现) */

void ui_tab_home_create(lv_obj_t *parent);
void ui_tab_wifi_create(lv_obj_t *parent);
void ui_tab_devices_create(lv_obj_t *parent);
void ui_tab_files_create(lv_obj_t *parent);
void ui_tab_music_create(lv_obj_t *parent);
void ui_tab_light_create(lv_obj_t *parent);

/* 统一卡片容器样式 */
lv_obj_t *ui_card(lv_obj_t *parent);

/* 公开 setter 的线程安全包装 */
static inline void ui_lock(void)   { if (g_ui_api->lock) g_ui_api->lock(); }
static inline void ui_unlock(void) { if (g_ui_api->unlock) g_ui_api->unlock(); }
