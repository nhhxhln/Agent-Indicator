#include "rgb_panel.h"

#include "board.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_additions.h" /* 3线 SPI(esp_lcd_panel_io_additions 组件) */
#include "esp_lcd_st7701.h"

static const char *TAG = "rgb_panel";
static esp_lcd_panel_handle_t s_panel = NULL;

#if USE_VENDOR_INIT
/* 面板厂商专属 ST7701S 初始化序列(转自 /ssd/zhe.yu/LCD_INIT_CMD.txt)。
 * 9-bit 原始格式 0x00XX=命令/0x01XX=数据 已拆成 {cmd,{data...},len,delay_ms}。
 * 组件会在本表前自发 0xFF(BK0)+0x36+0x3A;表尾保留 0x29/0x11(0x11 延时补到 120ms)。 */
static const st7701_lcd_init_cmd_t s_vendor_init[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xC3, (uint8_t[]){0x02}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0}, /* BK0 */
    {0xC0, (uint8_t[]){0x3B, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x10, 0x0C}, 2, 0},
    {0xC2, (uint8_t[]){0x07, 0x0A}, 2, 0},
    {0xC7, (uint8_t[]){0x00}, 1, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xCD, (uint8_t[]){0x08}, 1, 0},
    {0xB0, (uint8_t[]){0x05, 0x12, 0x98, 0x0E, 0x0F, 0x07, 0x07, 0x09,
                       0x09, 0x23, 0x05, 0x52, 0x0F, 0x67, 0x2C, 0x11}, 16, 0}, /* +Gamma */
    {0xB1, (uint8_t[]){0x0B, 0x11, 0x97, 0x0C, 0x12, 0x06, 0x06, 0x08,
                       0x08, 0x22, 0x03, 0x51, 0x11, 0x66, 0x2B, 0x0F}, 16, 0}, /* -Gamma */
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0}, /* BK1 */
    {0xB0, (uint8_t[]){0x5D}, 1, 0}, /* Vop=4.7375V */
    {0xB1, (uint8_t[]){0x35}, 1, 0}, /* VCOM=32 */
    {0xB2, (uint8_t[]){0x81}, 1, 0}, /* VGH=15V */
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4E}, 1, 0}, /* VGL=-10.17V */
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x20}, 1, 0}, /* AVDD=6.6 & AVCL=-4.6 */
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x00, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x06, 0x30, 0x08, 0x30, 0x05, 0x30, 0x07, 0x30,
                       0x00, 0x33, 0x33}, 11, 0},
    {0xE2, (uint8_t[]){0x11, 0x11, 0x33, 0x33, 0xF4, 0x00, 0x00, 0x00,
                       0xF4, 0x00, 0x00, 0x00}, 12, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x0D, 0xF5, 0x30, 0xF0, 0x0F, 0xF7, 0x30, 0xF0,
                       0x09, 0xF1, 0x30, 0xF0, 0x0B, 0xF3, 0x30, 0xF0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x0C, 0xF4, 0x30, 0xF0, 0x0E, 0xF6, 0x30, 0xF0,
                       0x08, 0xF0, 0x30, 0xF0, 0x0A, 0xF2, 0x30, 0xF0}, 16, 0},
    {0xE9, (uint8_t[]){0x36, 0x01}, 2, 0},
    {0xEB, (uint8_t[]){0x00, 0x01, 0xE4, 0xE4, 0x44, 0x88, 0x40}, 7, 0},
    {0xED, (uint8_t[]){0xFF, 0x10, 0xAF, 0x76, 0x54, 0x2B, 0xCF, 0xFF,
                       0xFF, 0xFC, 0xB2, 0x45, 0x67, 0xFA, 0x01, 0xFF}, 16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0}, /* 回 BK0 */
    {0x3A, (uint8_t[]){0x66}, 1, 0}, /* COLMOD = 18bit RGB666(16线接高位) */
    {0x36, (uint8_t[]){0x00}, 1, 0}, /* MADCTL */
    {0x35, (uint8_t[]){0x00}, 1, 0}, /* TE on(command2 关时组件会跳过,无害) */
    {0x29, (uint8_t[]){0x00}, 0, 20},  /* Display ON */
    {0x11, (uint8_t[]){0x00}, 0, 120}, /* Sleep Out(原 20ms,补到 120ms 更稳) */
};
#endif

esp_err_t rgb_panel_init(void)
{
    /* 1) ST7701S 初始化命令走 3 线 SPI(9-bit:CS/SCL/SDA 软件/GPIO 驱动) */
    spi_line_config_t line = {
        .cs_io_type = IO_TYPE_GPIO,
        .cs_gpio_num = LCD_SPI_CS,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = LCD_SPI_SCL,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = LCD_SPI_SDA,
        .io_expander = NULL,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_cfg = {
        .line_config = line,
        .expect_clk_speed = PANEL_IO_3WIRE_SPI_CLK_MAX,
        .spi_mode = 0,
        .lcd_cmd_bytes = 1,
        .lcd_param_bytes = 1,
        .flags = {
            .use_dc_bit = 1,
            .dc_zero_on_data = 0,
            .lsb_first = 0,
            .cs_high_active = 0,
            .del_keep_cs_inactive = 1,
        },
    };
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_cfg, &io), TAG, "3wire io");

    /* 2) RGB 并口配置(480×480,16bit=RGB565,双帧缓冲走 PSRAM) */
    esp_lcd_rgb_panel_config_t rgb_cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .data_width = 16,        /* RGB565 */
        .bits_per_pixel = 16,
        .num_fbs = 2,            /* 双缓冲,draw_bitmap 自动切换 */
        .bounce_buffer_size_px = PANEL_H_RES * 10,
        .de_gpio_num = RGB_DE,
        .pclk_gpio_num = RGB_PCLK,
        .vsync_gpio_num = RGB_VSYNC,
        .hsync_gpio_num = RGB_HSYNC,
        .disp_gpio_num = -1,
        .data_gpio_nums = {
            RGB_B0, RGB_B1, RGB_B2, RGB_B3, RGB_B4,
            RGB_G0, RGB_G1, RGB_G2, RGB_G3, RGB_G4, RGB_G5,
            RGB_R0, RGB_R1, RGB_R2, RGB_R3, RGB_R4,
        },
        .timings = {
            .pclk_hz = RGB_PCLK_HZ,
            .h_res = PANEL_H_RES,
            .v_res = PANEL_V_RES,
            .hsync_pulse_width = RGB_HSYNC_PULSE,
            .hsync_back_porch = RGB_HSYNC_BACK_PORCH,
            .hsync_front_porch = RGB_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = RGB_VSYNC_PULSE,
            .vsync_back_porch = RGB_VSYNC_BACK_PORCH,
            .vsync_front_porch = RGB_VSYNC_FRONT_PORCH,
            .flags.pclk_active_neg = true,
        },
        .flags.fb_in_psram = true,
    };

    /* 3) 用 ST7701 厂商封装包住 RGB(init_cmds=NULL → 用组件内置 ST7701S 默认初始化序列;
     *    面板不亮/偏色时把数据手册的初始化序列填进 init_cmds 覆盖)。 */
    st7701_vendor_config_t vendor_cfg = {
        .rgb_config = &rgb_cfg,
        .flags = {
            .auto_del_panel_io = 1, /* init 后自动释放 3 线 SPI io */
            .mirror_by_cmd = 1,
        },
    };
#if USE_VENDOR_INIT
    vendor_cfg.init_cmds = s_vendor_init; /* 面板厂商专属序列 */
    vendor_cfg.init_cmds_size = sizeof(s_vendor_init) / sizeof(s_vendor_init[0]);
    ESP_LOGI(TAG, "using vendor init (%d cmds)", vendor_cfg.init_cmds_size);
#else
    ESP_LOGI(TAG, "using component default init");
#endif
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7701(io, &panel_cfg, &s_panel), TAG, "st7701");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "disp on");

    ESP_LOGI(TAG, "ST7701S %dx%d RGB565 ready (pclk %d MHz)",
             PANEL_H_RES, PANEL_V_RES, RGB_PCLK_HZ / 1000000);
    return ESP_OK;
}

esp_err_t rgb_panel_draw(int x, int y, int w, int h, const void *px565)
{
    if (!s_panel) return ESP_ERR_INVALID_STATE;
    return esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, px565);
}

esp_err_t rgb_panel_blit_full(const uint16_t *fb565)
{
    return rgb_panel_draw(0, 0, PANEL_H_RES, PANEL_V_RES, fb565);
}
