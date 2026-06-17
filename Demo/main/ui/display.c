/* LCD 子系统(Demo):GC9A01 240×240 圆屏,SPI 接口。
 *   spi_bus(MOSI/SCLK)→ panel_io_spi(CS/DC)→ esp_lcd_gc9a01(RST 软复位)
 *   → esp_lvgl_port 挂 LVGL9 →(可选)CST816 触摸 → ui_screens。
 * 无触摸时用 BOARD_BTN(BOOT 键)轮播切页。背光默认常亮(BL 接 VCC)。
 * 字体:存在 /spiffs/fonts/ui.ttf 则用 TinyTTF,否则内置 Montserrat。 */
#include "ui/display.h"

#include <sys/stat.h>

#include "app_state.h"
#include "audio/audio.h"
#include "board.h"
#include "bus/i2c_bus.h"
#include "comm/comm.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "drivers/sensors.h"
#include "esp_check.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs.h"
#include "storage/storage.h"
#include "ui/i18n.h"
#include "ui/led_engine.h"
#include "ui/screens/screens.h"

static const char *TAG = "display";

extern bool qmi8658_present_c(void);
extern bool qmi8658_read_c(float *, float *, float *, float *);
extern bool audio_ready_c(void);
extern void audio_set_volume(int);

#define LCD_HOST       SPI2_HOST
#define TTF_PATH       "/spiffs/fonts/ui.ttf"
#define TTF_LV_PATH    "A:" TTF_PATH
#define TEXT_TASK_STACK 6144

static bool s_ready = false;
static lv_display_t *s_disp;
static const lv_font_t *s_font;
static int s_page;

static const uint32_t STATE_HEX[AGENT_STATE_MAX] = {
    [AGENT_IDLE] = 0x404060,        [AGENT_CONNECTING] = 0x0080cc,
    [AGENT_THINKING] = 0x9050ff,    [AGENT_RESPONDING] = 0x00cc88,
    [AGENT_TOOL_USE] = 0xee8800,    [AGENT_WAITING_USER] = 0x0090ee,
    [AGENT_ERROR] = 0xee2222,       [AGENT_RATE_LIMITED] = 0xcc6600,
    [AGENT_OFFLINE] = 0x555566,
};

bool display_ready(void) { return s_ready; }

/* GC9A01 背光默认常亮(BOARD_LCD_BL=-1 接 VCC);若接 GPIO 则简单开关 */
void display_set_backlight(int pct)
{
#if BOARD_LCD_BL >= 0
    gpio_set_level((gpio_num_t)BOARD_LCD_BL, pct > 0 ? 1 : 0);
#else
    (void)pct;
#endif
}

static esp_err_t panel_init(void)
{
    spi_bus_config_t buscfg = {
        .sclk_io_num = BOARD_LCD_SPI_SCLK,
        .mosi_io_num = BOARD_LCD_SPI_MOSI,
        .miso_io_num = BOARD_LCD_SPI_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_H_RES * 80 * (int)sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO),
                        TAG, "spi bus");

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = BOARD_LCD_SPI_CS,
        .dc_gpio_num = BOARD_LCD_SPI_DC,
        .spi_mode = 0,
        .pclk_hz = BOARD_LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io),
        TAG, "panel io");

    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t pcfg = {
        .reset_gpio_num = BOARD_LCD_RST, /* -1 软复位 */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(io, &pcfg, &panel), TAG, "gc9a01");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "init");
    esp_lcd_panel_invert_color(panel, true); /* GC9A01 需反色 */
    esp_lcd_panel_disp_on_off(panel, true);

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 10240; /* TinyTTF 栅格化在 LVGL 任务 */
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl port");

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = BOARD_LCD_H_RES * 40, /* 省 internal RAM(部分刷新) */
        .double_buffer = true,
        .hres = BOARD_LCD_H_RES,
        .vres = BOARD_LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = { .buff_dma = true, .swap_bytes = true },
    };
    s_disp = lvgl_port_add_disp(&disp_cfg);
    ESP_RETURN_ON_FALSE(s_disp, ESP_FAIL, TAG, "add disp");

    /* 触摸(可选:GC9A01 触摸版 CST816/CST836U @0x15,在位才加) */
    if (i2c_probe(I2C_ADDR_CST836U)) {
        esp_lcd_panel_io_handle_t tp_io;
        esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
        if (esp_lcd_new_panel_io_i2c(i2c_bus_handle(),
                                     &tp_io_cfg, &tp_io) == ESP_OK) {
            esp_lcd_touch_config_t tp_cfg = {
                .x_max = BOARD_LCD_H_RES,
                .y_max = BOARD_LCD_V_RES,
                .rst_gpio_num = -1,
                .int_gpio_num = -1,
            };
            esp_lcd_touch_handle_t tp;
            if (esp_lcd_touch_new_i2c_cst816s(tp_io, &tp_cfg, &tp) == ESP_OK) {
                lvgl_port_touch_cfg_t touch_cfg = { .disp = s_disp, .handle = tp };
                lvgl_port_add_touch(&touch_cfg);
                ESP_LOGI(TAG, "touch CST816/CST836U attached");
            }
        }
    } else {
        ESP_LOGI(TAG, "no touch, use BOOT button to switch pages");
    }
    return ESP_OK;
}

/* ---- ui_host_api_t 回调 ---- */
static void api_lock(void) { lvgl_port_lock(0); }
static void api_unlock(void) { lvgl_port_unlock(); }
static void api_wifi_scan(void) { comm_wifi_scan_async(); }
static void api_wifi_connect(const char *ssid, const char *pass)
{
    comm_wifi_set_credentials(ssid, pass);
}
static void api_music_cmd(int cmd)
{
    if (cmd == 1) audio_play_tone(0);
}
static void api_music_volume(int vol) { audio_set_volume(vol); }
static void api_light_set(int mode, uint8_t r, uint8_t g, uint8_t b,
                          int speed, int brightness)
{
    led_engine_set_fx((led_fx_t)mode, r, g, b, (uint8_t)speed);
    g_app.brightness = (uint8_t)(brightness * 255 / 100);
    display_set_backlight(brightness);
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
    ui_devices_set(UI_DEV_TOUCH, i2c_probe(I2C_ADDR_CST836U), "0x15");
    ui_devices_set(UI_DEV_IMU, qmi8658_present_c(), "0x6B");
    ui_devices_set(UI_DEV_CODEC, audio_ready_c(), audio_codec_name());
    ui_devices_set(UI_DEV_INA226, i2c_probe(I2C_ADDR_INA226), "0x40");
    ui_devices_set(UI_DEV_CHARGER, i2c_probe(I2C_ADDR_MP2760), "0x5C");
    ui_devices_set(UI_DEV_SD, storage_sd_mounted(),
                   storage_sd_mounted() ? "mounted" : "no card");
    ui_devices_set(UI_DEV_CAN, true, "500k");
    ui_devices_set(UI_DEV_SHT, sht4x_present(), "0x44");
    ui_devices_set(UI_DEV_BMP, bmp280_present(), "0x76");
    ui_devices_set(UI_DEV_RTC, pcf8563_present(), "0x51");
}

static void api_on_rebuild(void)
{
    devices_populate();
    ui_files_set_root("A:" STORAGE_SPIFFS_BASE);
    ui_music_set_track("(no track)", "put WAV in /sdcard/music", 0);
    ui_home_set_state(tr_state(g_app.state), lv_color_hex(STATE_HEX[g_app.state]));
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
        lv_font_t *ttf = lv_tiny_ttf_create_file(TTF_LV_PATH, 18);
        if (ttf) {
            s_font = ttf;
            ESP_LOGI(TAG, "ttf font loaded: %s", TTF_PATH);
        }
    }
#endif
    ui_set_font(s_font);
    lv_obj_t *root = ui_screens_create(&s_api, theme_load());
    lv_obj_set_style_text_font(root, s_font, 0);
    lvgl_port_unlock();

    ui_files_set_root("A:" STORAGE_SPIFFS_BASE);
    ui_home_append_text(tr(STR_WELCOME));
    ui_music_set_track("(no track)", "put WAV in /sdcard/music", 0);
    devices_populate();
}

/* BOOT 按钮轮播切页(无触摸时) */
static void button_task(void *arg)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOARD_BTN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    int last = 1;
    while (1) {
        int v = gpio_get_level(BOARD_BTN);
        if (last == 1 && v == 0 && s_ready) { /* 下降沿 */
            s_page = (s_page + 1) % 6;
            lvgl_port_lock(0);
            ui_screens_goto(s_page);
            lvgl_port_unlock();
            vTaskDelay(pdMS_TO_TICKS(250)); /* 消抖 */
        }
        last = v;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

/* UI 数据泵:文本流 + 状态/用量/context + IMU/传感器低速刷新 */
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
        if (s_ready && ++slow_div >= 25) {
            slow_div = 0;
            ui_home_set_usage(
                g_app.usage_pct[0] == 0xFF ? -1 : g_app.usage_pct[0],
                g_app.usage_pct[1] == 0xFF ? -1 : g_app.usage_pct[1]);
            ui_home_set_context(g_app.ctx_used / 1000, g_app.ctx_total / 1000);
            float ax, ay, az, tc;
            if (qmi8658_read_c(&ax, &ay, &az, &tc))
                ui_devices_set_imu_live(ax, ay, az, tc);
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
        if (s_ready) ui_home_append_text(buf);
        else ESP_LOGI(TAG, "[%s] %s", stream ? "out" : "in", buf);
    }
}

esp_err_t display_start(void)
{
    if (panel_init() == ESP_OK) {
        s_ready = true;
        ui_create();
        ESP_LOGI(TAG, "gc9a01 up 240x240");
    } else {
        ESP_LOGW(TAG, "lcd init failed, running headless");
    }
    xTaskCreate(text_task, "disp_text", TEXT_TASK_STACK, NULL, 5, NULL);
    xTaskCreate(button_task, "btn", 2560, NULL, 4, NULL);
    return ESP_OK;
}
