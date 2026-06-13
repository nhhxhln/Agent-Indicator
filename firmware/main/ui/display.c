/* LCD 子系统:ST7701 初始化链路(docs/04 §3)
 *   TCA9554 复位 → 3-wire SPI 写初始化序列(SCL 借 PCLK 线、SDA 借 G0 线、
 *   CS 在扩展器 P5,esp_lcd_panel_io_additions 实现)→ 引脚移交 LCD_CAM RGB565
 *   → esp_lvgl_port 挂 LVGL9 + CST820 触摸 → 背光。
 * 字体:若存在 /spiffs/fonts/ui.ttf 则用 TinyTTF 加载(otf 同样支持),
 *   否则回落内置 Montserrat。 */
#include "ui/display.h"

#include <sys/stat.h>

#include "app_state.h"
#include "audio/audio.h"
#include "board.h"
#include "bus/i2c_bus.h"
#include "comm/comm.h"
#include "esp_check.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "storage/storage.h"
#include "driver/ledc.h"
#include "drivers/sensors.h"
#include "nvs.h"
#include "ui/i18n.h"
#include "ui/led_engine.h"
#include "ui/screens/screens.h"

static const char *TAG = "display";

extern bool qmi8658_present_c(void);
extern bool qmi8658_read_c(float *, float *, float *, float *);
extern bool audio_ready_c(void);
extern void audio_set_volume(int);

#define TTF_PATH      "/spiffs/fonts/ui.ttf" /* stat 用 VFS 路径 */
#define TTF_LV_PATH   "A:" TTF_PATH          /* LVGL FS_POSIX 盘符 A: */
#define TEXT_TASK_STACK 6144                 /* LVGL 调用 + 文本拼接 */

static bool s_ready = false;
static lv_display_t *s_disp;
static const lv_font_t *s_font;

/* 状态主题色(与 led_engine 一致) */
static const uint32_t STATE_HEX[AGENT_STATE_MAX] = {
    [AGENT_IDLE] = 0x404060,        [AGENT_CONNECTING] = 0x0080cc,
    [AGENT_THINKING] = 0x9050ff,    [AGENT_RESPONDING] = 0x00cc88,
    [AGENT_TOOL_USE] = 0xee8800,    [AGENT_WAITING_USER] = 0x0090ee,
    [AGENT_ERROR] = 0xee2222,       [AGENT_RATE_LIMITED] = 0xcc6600,
    [AGENT_OFFLINE] = 0x555566,
};

bool display_ready(void) { return s_ready; }

/* 背光:LEDC PWM → 恒流驱动 IC 的 DIM 脚(亮度调节);EN 走 TCA9554 */
static bool s_bl_inited = false;
static void backlight_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);
    ledc_channel_config_t c = {
        .gpio_num = BOARD_LCD_BL_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&c);
    s_bl_inited = true;
}

void display_set_backlight(int pct)
{
    if (!s_bl_inited) return; /* 无屏时空操作 */
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pct * 255 / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static esp_err_t panel_init(void)
{
    ESP_RETURN_ON_FALSE(io_expander() != NULL, ESP_ERR_NOT_FOUND, TAG,
                        "no io expander, no lcd");
    /* 复位时序 */
    io_expander_set(EXP_LCD_RST, false);
    io_expander_set(EXP_TP_RST, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    io_expander_set(EXP_LCD_RST, true);
    io_expander_set(EXP_TP_RST, true);
    vTaskDelay(pdMS_TO_TICKS(120));

    /* 1. 3-wire SPI(借用 RGB 引脚)写初始化序列 */
    spi_line_config_t line = {
        .cs_io_type = IO_TYPE_EXPANDER,
        .cs_expander_pin = IO_EXPANDER_PIN_NUM_5,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = BOARD_LCD_PCLK,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = BOARD_LCD_D_G0,
        .io_expander = io_expander(),
    };
    esp_lcd_panel_io_3wire_spi_config_t io_cfg =
        ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line, 0);
    esp_lcd_panel_io_handle_t io;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_cfg, &io), TAG, "3wire");

    /* 2. RGB565 面板 */
    esp_lcd_rgb_panel_config_t rgb_cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = ST7701_480_480_PANEL_60HZ_RGB_TIMING(), /* TODO: 按屏厂参数核对 */
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 2,
        .bounce_buffer_size_px = BOARD_LCD_H_RES * 10,
        .psram_trans_align = 64,
        .hsync_gpio_num = BOARD_LCD_HSYNC,
        .vsync_gpio_num = BOARD_LCD_VSYNC,
        .de_gpio_num = BOARD_LCD_DE,
        .pclk_gpio_num = BOARD_LCD_PCLK,
        .disp_gpio_num = -1,
        .data_gpio_nums = {
            BOARD_LCD_D_B0, BOARD_LCD_D_B1, BOARD_LCD_D_B2, BOARD_LCD_D_B3,
            BOARD_LCD_D_B4, BOARD_LCD_D_G0, BOARD_LCD_D_G1, BOARD_LCD_D_G2,
            BOARD_LCD_D_G3, BOARD_LCD_D_G4, BOARD_LCD_D_G5, BOARD_LCD_D_R0,
            BOARD_LCD_D_R1, BOARD_LCD_D_R2, BOARD_LCD_D_R3, BOARD_LCD_D_R4,
        },
        .flags = { .fb_in_psram = 1 },
    };
    st7701_vendor_config_t vendor = {
        .rgb_config = &rgb_cfg,
        .init_cmds = NULL, /* 屏厂序列拿到后替换,先用组件默认 */
        .init_cmds_size = 0,
        .flags = { .auto_del_panel_io = 1, .mirror_by_cmd = 0 },
    };
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1, /* 复位走扩展器 */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    esp_lcd_panel_handle_t panel;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7701(io, &panel_cfg, &panel), TAG, "st7701");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "init");

    /* 3. LVGL */
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 10240; /* TinyTTF 栅格化在 LVGL 任务上,放大栈 */
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl port");

    lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = panel,
        .buffer_size = BOARD_LCD_H_RES * BOARD_LCD_V_RES,
        .double_buffer = true,
        .hres = BOARD_LCD_H_RES,
        .vres = BOARD_LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = { .buff_dma = false, .buff_spiram = false, .direct_mode = true },
    };
    lvgl_port_display_rgb_cfg_t rgb_port_cfg = {
        .flags = { .bb_mode = true, .avoid_tearing = true },
    };
    s_disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_port_cfg);
    ESP_RETURN_ON_FALSE(s_disp, ESP_FAIL, TAG, "add disp");

    /* 4. 触摸(CST820 用 cst816s 驱动,轮询) */
    esp_lcd_panel_io_handle_t tp_io;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    if (esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)(uintptr_t)i2c_bus_port(),
                                 &tp_io_cfg, &tp_io) == ESP_OK) {
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = BOARD_LCD_H_RES,
            .y_max = BOARD_LCD_V_RES,
            .rst_gpio_num = -1,
            .int_gpio_num = -1, /* INT 在扩展器,直接轮询 */
        };
        esp_lcd_touch_handle_t tp;
        if (esp_lcd_touch_new_i2c_cst816s(tp_io, &tp_cfg, &tp) == ESP_OK) {
            lvgl_port_touch_cfg_t touch_cfg = { .disp = s_disp, .handle = tp };
            lvgl_port_add_touch(&touch_cfg);
        } else {
            ESP_LOGW(TAG, "cst820 not found, touch disabled");
        }
    }

    io_expander_set(EXP_LCD_BL_EN, true); /* 恒流 IC EN */
    backlight_init();
    display_set_backlight(80);
    return ESP_OK;
}

/* ---- ui_host_api_t 回调:接固件后端 ---- */
static void api_lock(void) { lvgl_port_lock(0); }
static void api_unlock(void) { lvgl_port_unlock(); }
static void api_wifi_scan(void) { comm_wifi_scan_async(); }
static void api_wifi_connect(const char *ssid, const char *pass)
{
    comm_wifi_set_credentials(ssid, pass);
}
static void api_music_cmd(int cmd)
{
    /* TODO: 接 SD 播放列表;骨架先以提示音反馈按键 */
    if (cmd == 1) audio_play_tone(0);
}
static void api_music_volume(int vol) { audio_set_volume(vol); }
static void api_light_set(int mode, uint8_t r, uint8_t g, uint8_t b,
                          int speed, int brightness)
{
    led_engine_set_fx((led_fx_t)mode, r, g, b, (uint8_t)speed);
    g_app.brightness = (uint8_t)(brightness * 255 / 100);
    display_set_backlight(brightness); /* 亮度滑条同时调 LCD 背光 */
}

static void theme_persist(int dark)
{
    nvs_handle_t h;
    if (nvs_open("ui", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "dark", (uint8_t)dark);
        nvs_commit(h);
        nvs_close(h);
    }
}
static int theme_load(void)
{
    nvs_handle_t h;
    uint8_t v = 1;
    if (nvs_open("ui", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, "dark", &v);
        nvs_close(h);
    }
    return v;
}

static void devices_populate(void)
{
    ui_devices_set(UI_DEV_EXPANDER, io_expander() != NULL, "0x20");
    ui_devices_set(UI_DEV_TOUCH, i2c_probe(I2C_ADDR_CST820), "0x15");
    ui_devices_set(UI_DEV_IMU, qmi8658_present_c(), "0x6B");
    ui_devices_set(UI_DEV_CODEC, audio_ready_c(), "I2S");
    ui_devices_set(UI_DEV_INA226, i2c_probe(I2C_ADDR_INA226), "0x40");
    ui_devices_set(UI_DEV_CHARGER, i2c_probe(I2C_ADDR_MP2760), "0x5C");
    ui_devices_set(UI_DEV_SD, storage_sd_mounted(),
                   storage_sd_mounted() ? "mounted" : "no card");
    ui_devices_set(UI_DEV_CAN, true, "500k"); /* 驱动已装;总线活动见 can_status */
    ui_devices_set(UI_DEV_SHT, sht4x_present(), "0x44");
    ui_devices_set(UI_DEV_BMP, bmp280_present(), "0x76");
    ui_devices_set(UI_DEV_RTC, pcf8563_present(), "0x51");
}

static void api_on_rebuild(void)
{
    /* 主题切换重建后重填静态数据(文本流历史不恢复) */
    devices_populate();
    ui_files_set_root("A:" STORAGE_SPIFFS_BASE);
    ui_music_set_track("(no track)", "put WAV in /sdcard/music", 0);
    ui_home_set_state(tr_state(g_app.state),
                      lv_color_hex(STATE_HEX[g_app.state]));
}

static const ui_host_api_t s_api = {
    .lock = api_lock,
    .unlock = api_unlock,
    .wifi_scan = api_wifi_scan,
    .wifi_connect = api_wifi_connect,
    .music_cmd = api_music_cmd,
    .music_volume = api_music_volume,
    .light_set = api_light_set,
    .theme_persist = theme_persist,
    .on_rebuild = api_on_rebuild,
};

static void ui_create(void)
{
    lvgl_port_lock(0);
    s_font = LV_FONT_DEFAULT;
#if LV_USE_TINY_TTF
    struct stat st;
    if (stat(TTF_PATH, &st) == 0) {
        lv_font_t *ttf = lv_tiny_ttf_create_file(TTF_LV_PATH, 22);
        if (ttf) {
            s_font = ttf;
            ESP_LOGI(TAG, "ttf font loaded: %s", TTF_PATH);
        }
    }
#endif
    ui_set_font(s_font); /* build() 内据此 init theme 字体 */
    lv_obj_t *root = ui_screens_create(&s_api, theme_load());
    lv_obj_set_style_text_font(root, s_font, 0);
    lvgl_port_unlock();

    ui_files_set_root("A:" STORAGE_SPIFFS_BASE);
    ui_home_append_text(tr(STR_WELCOME));
    ui_music_set_track("(no track)", "put WAV in /sdcard/music", 0);
    devices_populate();
}

/* UI 数据泵:文本流 + 状态/用量/context 同步 + IMU/遥测低速刷新 */
static void text_task(void *arg)
{
    char buf[257];
    uint8_t stream;
    agent_state_t last_state = AGENT_STATE_MAX;
    int slow_div = 0;
    while (1) {
        if (s_ready && g_app.state != last_state) {
            last_state = g_app.state;
            ui_home_set_state(tr_state(last_state),
                              lv_color_hex(STATE_HEX[last_state]));
        }
        if (s_ready && ++slow_div >= 25) { /* 500ms 低速项 */
            slow_div = 0;
            ui_home_set_usage(
                g_app.usage_pct[0] == 0xFF ? -1 : g_app.usage_pct[0],
                g_app.usage_pct[1] == 0xFF ? -1 : g_app.usage_pct[1]);
            ui_home_set_context(g_app.ctx_used / 1000, g_app.ctx_total / 1000);
            float ax, ay, az, tc;
            if (qmi8658_read_c(&ax, &ay, &az, &tc))
                ui_devices_set_imu_live(ax, ay, az, tc);
            /* 环境读数:SHT4x 温湿度优先,气压取 BMP280 */
            float temp = 0, humi = 0, press = 0, bt;
            bool ok = sht4x_read(&temp, &humi);
            if (bmp280_read(&press, &bt) && !ok) temp = bt;
            if (ok || press > 0) ui_devices_set_env(temp, humi, press);
            char clk[16];
            if (pcf8563_read(clk, sizeof(clk))) ui_devices_set_time(clk);
        }
        int n = app_text_pop(buf, sizeof(buf) - 1, &stream);
        if (n <= 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        buf[n] = '\0';
        if (s_ready) {
            ui_home_append_text(buf);
        } else {
            ESP_LOGI(TAG, "[%s] %s", stream ? "out" : "in", buf);
        }
    }
}

esp_err_t display_start(void)
{
    if (panel_init() == ESP_OK) {
        s_ready = true;
        ui_create();
        ESP_LOGI(TAG, "lcd up 480x480");
    } else {
        ESP_LOGW(TAG, "lcd init failed, running headless");
    }
    xTaskCreate(text_task, "disp_text", TEXT_TASK_STACK, NULL, 5, NULL);
    return ESP_OK;
}
