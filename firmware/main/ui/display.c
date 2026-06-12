/* LCD 子系统:ST7701 初始化链路(docs/04 §3)
 *   TCA9554 复位 → 3-wire SPI 写初始化序列(SCL 借 PCLK 线、SDA 借 G0 线、
 *   CS 在扩展器 P5,esp_lcd_panel_io_additions 实现)→ 引脚移交 LCD_CAM RGB565
 *   → esp_lvgl_port 挂 LVGL9 + CST820 触摸 → 背光。
 * 字体:若存在 /spiffs/fonts/ui.ttf 则用 TinyTTF 加载(otf 同样支持),
 *   否则回落内置 Montserrat。 */
#include "ui/display.h"

#include <sys/stat.h>

#include "app_state.h"
#include "board.h"
#include "bus/i2c_bus.h"
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

static const char *TAG = "display";

#define TTF_PATH      "/spiffs/fonts/ui.ttf" /* stat 用 VFS 路径 */
#define TTF_LV_PATH   "A:" TTF_PATH          /* LVGL FS_POSIX 盘符 A: */
#define TEXT_TASK_STACK 6144                 /* LVGL 调用 + 文本拼接 */

static bool s_ready = false;
static lv_display_t *s_disp;
static lv_obj_t *s_textarea;
static const lv_font_t *s_font;

bool display_ready(void) { return s_ready; }

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

    io_expander_set(EXP_LCD_BL_EN, true);
    return ESP_OK;
}

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
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0a0a12), 0);
    s_textarea = lv_textarea_create(scr);
    lv_obj_set_size(s_textarea, BOARD_LCD_H_RES - 20, BOARD_LCD_V_RES - 20);
    lv_obj_center(s_textarea);
    lv_obj_set_style_bg_color(s_textarea, lv_color_hex(0x0a0a12), 0);
    lv_obj_set_style_text_color(s_textarea, lv_color_hex(0xd0d0e0), 0);
    lv_obj_set_style_text_font(s_textarea, s_font, 0);
    lv_textarea_set_max_length(s_textarea, 4096);
    lvgl_port_unlock();
}

/* 文本流消费:host TEXT 消息 → 屏幕(或日志降级) */
static void text_task(void *arg)
{
    char buf[257];
    uint8_t stream;
    while (1) {
        int n = app_text_pop(buf, sizeof(buf) - 1, &stream);
        if (n <= 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        buf[n] = '\0';
        if (s_ready && s_textarea) {
            lvgl_port_lock(0);
            lv_textarea_add_text(s_textarea, buf);
            lvgl_port_unlock();
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
