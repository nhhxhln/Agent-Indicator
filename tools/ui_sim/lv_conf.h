/* ui_sim 的 LVGL 配置(PC 无头渲染)。
 * 只覆盖与固件 sdkconfig 对齐的项,其余用 lv_conf_internal 默认值。 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 32

/* 用系统 malloc,内置 64KB TLSF 池撑不下六页 UI(OOM 后 realloc 死循环) */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_USE_FS_POSIX 1
#define LV_FS_POSIX_LETTER 'A'
#define LV_FS_POSIX_PATH ""

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1

#define LV_USE_LOG 0

#endif /* LV_CONF_H */
