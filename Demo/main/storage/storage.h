/* 文件系统:SPIFFS(分区 storage → /spiffs)+ microSD(1-bit SDMMC → /sdcard)。
 * LVGL 经 LV_USE_FS_POSIX(盘符 A:)直接访问两者,如 "A:/spiffs/fonts/ui.ttf"。 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_SPIFFS_BASE "/spiffs"
#define STORAGE_SD_BASE     "/sdcard"

esp_err_t storage_init(void);     /* 挂 SPIFFS(必选)+ 尝试挂 SD(可热插) */
bool storage_sd_mounted(void);
esp_err_t storage_sd_mount(void); /* 控制台 `sd mount` 用 */
esp_err_t storage_sd_unmount(void);

#ifdef __cplusplus
}
#endif
