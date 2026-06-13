/* UI 模拟器:无头渲染固件界面(XRGB8888)→ 每个 Tab 导出一张 BMP。
 * 用途:文档截图、PC 上快速迭代 UI,无需烧录。 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lvgl.h"
#include "screens.h"

#define HOR 480
#define VER 480

static uint8_t s_fb[HOR * VER * 4];

static uint32_t tick_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    (void)area; (void)px; /* DIRECT 模式,s_fb 即整帧 */
    lv_display_flush_ready(disp);
}

/* XRGB8888 → 24bit BMP(行自底向上) */
static void dump_bmp(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    int row = HOR * 3, img = row * VER;
    uint8_t hdr[54] = { 'B', 'M' };
    uint32_t fsz = 54 + img, off = 54, hsz = 40, w = HOR, h = VER;
    uint16_t planes = 1, bpp = 24;
    memcpy(hdr + 2, &fsz, 4); memcpy(hdr + 10, &off, 4);
    memcpy(hdr + 14, &hsz, 4); memcpy(hdr + 18, &w, 4); memcpy(hdr + 22, &h, 4);
    memcpy(hdr + 26, &planes, 2); memcpy(hdr + 28, &bpp, 2);
    memcpy(hdr + 34, &img, 4);
    fwrite(hdr, 1, 54, f);
    for (int y = VER - 1; y >= 0; y--) {
        for (int x = 0; x < HOR; x++) {
            const uint8_t *p = s_fb + (y * HOR + x) * 4; /* B G R X */
            fwrite(p, 1, 3, f);
        }
    }
    fclose(f);
    printf("wrote %s\n", path);
}

static void settle(int ms)
{
    for (int t = 0; t < ms; t += 5) {
        lv_timer_handler();
        usleep(5000);
    }
}

/* ---- mock 数据 ---- */
static void mock_wifi_scan(void)
{
    ui_wifi_clear_networks();
    ui_wifi_add_network("agentind-ap", -42, true);
    ui_wifi_add_network("HomeLab-5G", -58, true);
    ui_wifi_add_network("CoffeeShop", -71, false);
}

static void feed_mock(void); /* fwd */
static void mock_on_rebuild(void) { feed_mock(); }
static const ui_host_api_t s_api = {
    .wifi_scan = mock_wifi_scan,
    .on_rebuild = mock_on_rebuild,
};

static void feed_mock(void)
{
    ui_home_set_state("Thinking", lv_color_hex(0x9050ff));
    ui_home_set_usage(62, 38);
    ui_home_set_context(128, 200);
    ui_home_append_text("> refactor the LED engine to support 16x16 tiling\n\n"
                        "Reading led_engine.c...\n"
                        "The matrix mapper already handles serpentine scan; "
                        "adding a 2x2 tile layer on top. Updating "
                        "matrix_index() and the current limiter budget...\n");

    ui_wifi_set_status("connected 192.168.4.2");
    mock_wifi_scan();

    ui_devices_set(UI_DEV_EXPANDER, true, "0x20");
    ui_devices_set(UI_DEV_TOUCH, true, "0x15");
    ui_devices_set(UI_DEV_IMU, true, "0x6B");
    ui_devices_set(UI_DEV_CODEC, true, "WM8960");
    ui_devices_set(UI_DEV_INA226, true, "0x40");
    ui_devices_set(UI_DEV_CHARGER, false, "--");
    ui_devices_set(UI_DEV_SD, true, "29.7GB");
    ui_devices_set(UI_DEV_CAN, true, "500k");
    ui_devices_set(UI_DEV_SHT, true, "0x44");
    ui_devices_set(UI_DEV_BMP, true, "0x76");
    ui_devices_set(UI_DEV_RTC, true, "0x51");
    ui_devices_set_imu_live(0.02f, -0.01f, 1.00f, 31.2f);
    ui_devices_set_power(11412, -324, 78);
    ui_devices_set_env(24.6f, 47.0f, 1013.0f);
    ui_devices_set_time("14:32:08");

    /* files:准备示例目录 */
    mkdir("/tmp/uisim_files", 0755);
    mkdir("/tmp/uisim_files/fonts", 0755);
    mkdir("/tmp/uisim_files/music", 0755);
    FILE *f = fopen("/tmp/uisim_files/boot.wav", "w");
    if (f) { fputs("x", f); fclose(f); }
    f = fopen("/tmp/uisim_files/config.json", "w");
    if (f) { fputs("{}", f); fclose(f); }
    ui_files_set_root("A:/tmp/uisim_files");

    ui_music_set_track("Weightless", "Marconi Union", 485);
    ui_music_set_position(152, 1);
}

static void shoot_all(const char *out, const char *prefix)
{
    static const char *names[6] = { "home", "light", "wifi",
                                    "devices", "files", "music" };
    char path[256];
    for (int i = 0; i < 6; i++) {
        ui_screens_goto(i);
        settle(300);
        snprintf(path, sizeof(path), "%s/ui-%s%s.bmp", out, prefix, names[i]);
        dump_bmp(path);
    }
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : ".";

    lv_init();
    lv_tick_set_cb(tick_ms);

    lv_display_t *disp = lv_display_create(HOR, VER);
    lv_display_set_buffers(disp, s_fb, NULL, sizeof(s_fb),
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                          lv_palette_main(LV_PALETTE_CYAN), true,
                          &lv_font_montserrat_16);

    ui_screens_create(&s_api, 1); /* 先暗色 */
    feed_mock();
    shoot_all(out, "");

    ui_screens_set_theme(0);      /* 切亮色,on_rebuild 重填数据 */
    shoot_all(out, "light-");
    return 0;
}
