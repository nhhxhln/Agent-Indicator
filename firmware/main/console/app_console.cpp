#include "app_console.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>

#include "esp_check.h"
#include "esp_console.h"
#include "esp_log.h"
#include "storage/storage.h"

static const char *TAG = "console";

/* ---- storage 命令(简单,直接放这里) ---- */

static int cmd_sd(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "mount") == 0)
        return storage_sd_mount() == ESP_OK ? 0 : 1;
    if (argc >= 2 && strcmp(argv[1], "umount") == 0)
        return storage_sd_unmount() == ESP_OK ? 0 : 1;
    printf("sd: %s\n", storage_sd_mounted() ? "mounted" : "not mounted");
    return 0;
}

static int cmd_ls(int argc, char **argv)
{
    const char *path = argc >= 2 ? argv[1] : STORAGE_SPIFFS_BASE;
    DIR *dir = opendir(path);
    if (!dir) {
        printf("opendir %s failed\n", path);
        return 1;
    }
    struct dirent *e;
    while ((e = readdir(dir)) != nullptr)
        printf("  %s%s\n", e->d_name, e->d_type == DT_DIR ? "/" : "");
    closedir(dir);
    return 0;
}

extern "C" void case_storage_register(void)
{
    const esp_console_cmd_t cmds[] = {
        { .command = "sd", .help = "sd [mount|umount] - SD 卡挂载管理",
          .hint = nullptr, .func = cmd_sd, .argtable = nullptr },
        { .command = "ls", .help = "ls [path] - 列目录(默认 /spiffs)",
          .hint = nullptr, .func = cmd_ls, .argtable = nullptr },
    };
    for (auto &c : cmds) ESP_ERROR_CHECK(esp_console_cmd_register(&c));
}

extern "C" esp_err_t app_console_start(void)
{
    esp_console_repl_t *repl = nullptr;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "agentind>";
    repl_cfg.task_stack_size = 8192; /* 命令直接跑在 REPL 任务上,printf+vfs 需要余量 */

    esp_console_dev_usb_serial_jtag_config_t hw_cfg =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(
        esp_console_new_repl_usb_serial_jtag(&hw_cfg, &repl_cfg, &repl), TAG, "repl");

    esp_console_register_help_command();
    case_storage_register();
    case_can_register();
    case_audio_register();
    case_imu_register();

    ESP_RETURN_ON_ERROR(esp_console_start_repl(repl), TAG, "start");
    ESP_LOGI(TAG, "console ready, type `help`");
    return ESP_OK;
}
