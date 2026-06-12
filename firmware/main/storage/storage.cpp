#include "storage.h"

#include "board.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "storage";

static sdmmc_card_t *s_card = nullptr;

extern "C" esp_err_t storage_init(void)
{
    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = STORAGE_SPIFFS_BASE;
    conf.partition_label = "storage";
    conf.max_files = 8;
    conf.format_if_mount_failed = true;
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spiffs mount failed: %s", esp_err_to_name(err));
        return err;
    }
    size_t total = 0, used = 0;
    esp_spiffs_info("storage", &total, &used);
    ESP_LOGI(TAG, "spiffs %u/%u KB used", (unsigned)(used / 1024), (unsigned)(total / 1024));

    if (storage_sd_mount() != ESP_OK)
        ESP_LOGW(TAG, "sd not mounted (no card?), use console `sd mount` later");
    return ESP_OK;
}

extern "C" bool storage_sd_mounted(void) { return s_card != nullptr; }

extern "C" esp_err_t storage_sd_mount(void)
{
    if (s_card) return ESP_OK;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT; /* GPIO 紧张,1-bit 模式(docs/04 §2) */
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk = (gpio_num_t)BOARD_SD_CLK;
    slot.cmd = (gpio_num_t)BOARD_SD_CMD;
    slot.d0 = (gpio_num_t)BOARD_SD_D0;
    slot.width = 1;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mnt = {};
    mnt.format_if_mount_failed = false;
    mnt.max_files = 8;
    mnt.allocation_unit_size = 16 * 1024;

    esp_err_t err = esp_vfs_fat_sdmmc_mount(STORAGE_SD_BASE, &host, &slot, &mnt, &s_card);
    if (err != ESP_OK) {
        s_card = nullptr;
        return err;
    }
    ESP_LOGI(TAG, "sd mounted: %s %lluMB", s_card->cid.name,
             ((uint64_t)s_card->csd.capacity * s_card->csd.sector_size) >> 20);
    return ESP_OK;
}

extern "C" esp_err_t storage_sd_unmount(void)
{
    if (!s_card) return ESP_OK;
    esp_err_t err = esp_vfs_fat_sdcard_unmount(STORAGE_SD_BASE, s_card);
    s_card = nullptr;
    return err;
}
