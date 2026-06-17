#include "app_console.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>

#include "esp_check.h"
#include "esp_console.h"
#include "esp_log.h"
#include "notify.h"
#include "storage/storage.h"
#include "ui/i18n.h"
#include "ui/led_engine.h"

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

static int cmd_lang(int argc, char **argv)
{
    if (argc >= 2) {
        if (strcmp(argv[1], "zh") == 0) ui_lang_set(UI_LANG_ZH);
        else if (strcmp(argv[1], "en") == 0) ui_lang_set(UI_LANG_EN);
        else { printf("usage: lang [zh|en]\n"); return 1; }
    }
    printf("lang=%s\n", ui_lang_get() == UI_LANG_ZH ? "zh" : "en");
    return 0;
}

static int cmd_light(int argc, char **argv)
{
    static const char *names[LED_FX_MAX] = { "agent", "solid", "breath",
                                             "marquee", "rainbow", "off" };
    if (argc < 2) {
        printf("light [all|matrix|aux] <agent|solid|breath|marquee|rainbow|off> "
               "[RRGGBB] [speed]\n"
               "current: matrix=%s aux=%s\n",
               names[led_engine_get_fx_target(LED_TGT_MATRIX)],
               names[led_engine_get_fx_target(LED_TGT_AUX)]);
        return 0;
    }
    /* 可选首参指定目标灯带 */
    int a = 1;
    led_target_t tg = LED_TGT_ALL;
    if (strcmp(argv[1], "all") == 0) { tg = LED_TGT_ALL; a = 2; }
    else if (strcmp(argv[1], "matrix") == 0) { tg = LED_TGT_MATRIX; a = 2; }
    else if (strcmp(argv[1], "aux") == 0) { tg = LED_TGT_AUX; a = 2; }
    if (a >= argc) { printf("missing mode\n"); return 1; }

    int mode = -1;
    for (int i = 0; i < LED_FX_MAX; i++)
        if (strcmp(argv[a], names[i]) == 0) mode = i;
    if (mode < 0) { printf("unknown mode\n"); return 1; }
    unsigned rgb = 0x0080ff;
    if (argc > a + 1) sscanf(argv[a + 1], "%x", &rgb);
    int speed = argc > a + 2 ? atoi(argv[a + 2]) : 40;
    led_engine_set_fx_target(tg, (led_fx_t)mode, (rgb >> 16) & 0xFF,
                             (rgb >> 8) & 0xFF, rgb & 0xFF, (uint8_t)speed);
    const char *tgn = tg == LED_TGT_MATRIX ? "matrix" : tg == LED_TGT_AUX ? "aux" : "all";
    printf("light[%s]=%s color=%06X speed=%d\n", tgn, names[mode], rgb, speed);
    return 0;
}

extern "C" void case_storage_register(void)
{
    const esp_console_cmd_t cmds[] = {
        { .command = "sd", .help = "sd [mount|umount] - SD 卡挂载管理",
          .hint = nullptr, .func = cmd_sd, .argtable = nullptr },
        { .command = "ls", .help = "ls [path] - 列目录(默认 /spiffs)",
          .hint = nullptr, .func = cmd_ls, .argtable = nullptr },
        { .command = "lang", .help = "lang [zh|en] - UI 语言(NVS 持久化)",
          .hint = nullptr, .func = cmd_lang, .argtable = nullptr },
        { .command = "light", .help = "light <mode> [RRGGBB] [speed] - 灯效",
          .hint = nullptr, .func = cmd_light, .argtable = nullptr },
    };
    for (auto &c : cmds) ESP_ERROR_CHECK(esp_console_cmd_register(&c));
}

extern "C" esp_err_t app_console_start(void)
{
    esp_console_repl_t *repl = nullptr;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "agentind>";
    repl_cfg.task_stack_size = 8192; /* 命令直接跑在 REPL 任务上,printf+vfs 需要余量 */

    /* REPL 跟随 sdkconfig 的控制台通道:默认 UART0(43/44,与 TWAI 二选一),
     * CAN 联调时 menuconfig 切回 USB Serial/JTAG */
#if CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
    esp_console_dev_uart_config_t hw_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(
        esp_console_new_repl_uart(&hw_cfg, &repl_cfg, &repl), TAG, "repl");
#else
    esp_console_dev_usb_serial_jtag_config_t hw_cfg =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(
        esp_console_new_repl_usb_serial_jtag(&hw_cfg, &repl_cfg, &repl), TAG, "repl");
#endif

    esp_console_register_help_command();
    case_storage_register();
    case_can_register();
    case_audio_register();
    case_imu_register();
    case_i2c_register();
    notify_register();

    ESP_RETURN_ON_ERROR(esp_console_start_repl(repl), TAG, "start");
    ESP_LOGI(TAG, "console ready, type `help`");
    return ESP_OK;
}
