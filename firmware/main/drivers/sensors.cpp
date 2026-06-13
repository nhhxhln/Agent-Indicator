#include "drivers/sensors.h"

#include <cstdio>
#include <cstring>

#include "bus/i2c_bus.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sensors";

namespace {

constexpr uint8_t ADDR_SHT4X = 0x44;
constexpr uint8_t ADDR_BMP280 = 0x76; /* SDO=GND;0x77 为 SDO=VDD */
constexpr uint8_t ADDR_PCF8563 = 0x51;

bool s_sht = false, s_bmp = false, s_rtc = false;

/* BMP280 出厂校准系数 */
struct {
    uint16_t T1; int16_t T2, T3;
    uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9;
} cal;

esp_err_t wr(uint8_t addr, const uint8_t *d, size_t n)
{
    return i2c_master_write_to_device(i2c_bus_port(), addr, d, n, pdMS_TO_TICKS(50));
}
esp_err_t rd_reg(uint8_t addr, uint8_t reg, uint8_t *d, size_t n)
{
    return i2c_master_write_read_device(i2c_bus_port(), addr, &reg, 1, d, n,
                                        pdMS_TO_TICKS(50));
}

void bmp280_load_cal(void)
{
    uint8_t b[24];
    if (rd_reg(ADDR_BMP280, 0x88, b, 24) != ESP_OK) return;
    cal.T1 = b[0] | (b[1] << 8);   cal.T2 = b[2] | (b[3] << 8);
    cal.T3 = b[4] | (b[5] << 8);   cal.P1 = b[6] | (b[7] << 8);
    cal.P2 = b[8] | (b[9] << 8);   cal.P3 = b[10] | (b[11] << 8);
    cal.P4 = b[12] | (b[13] << 8); cal.P5 = b[14] | (b[15] << 8);
    cal.P6 = b[16] | (b[17] << 8); cal.P7 = b[18] | (b[19] << 8);
    cal.P8 = b[20] | (b[21] << 8); cal.P9 = b[22] | (b[23] << 8);
}

uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

} // namespace

void sensors_init(void)
{
    s_sht = i2c_probe(ADDR_SHT4X);
    s_rtc = i2c_probe(ADDR_PCF8563);
    s_bmp = i2c_probe(ADDR_BMP280);
    if (s_bmp) {
        bmp280_load_cal();
        uint8_t ctrl[2] = { 0xF4, 0x27 }; /* 常态测量,温压 oversampling x1 */
        wr(ADDR_BMP280, ctrl, 2);
        uint8_t cfg[2] = { 0xF5, 0xA0 };  /* standby 1s */
        wr(ADDR_BMP280, cfg, 2);
    }
    ESP_LOGI(TAG, "sht4x=%d bmp280=%d pcf8563=%d", s_sht, s_bmp, s_rtc);
}

bool sht4x_present(void) { return s_sht; }
bool bmp280_present(void) { return s_bmp; }
bool pcf8563_present(void) { return s_rtc; }

bool sht4x_read(float *temp_c, float *humi_pct)
{
    if (!s_sht) return false;
    uint8_t cmd = 0xFD; /* 高精度测量 */
    if (wr(ADDR_SHT4X, &cmd, 1) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t b[6];
    if (i2c_master_read_from_device(i2c_bus_port(), ADDR_SHT4X, b, 6,
                                    pdMS_TO_TICKS(50)) != ESP_OK)
        return false;
    uint16_t t = (b[0] << 8) | b[1], h = (b[3] << 8) | b[4];
    *temp_c = -45.0f + 175.0f * t / 65535.0f;
    *humi_pct = -6.0f + 125.0f * h / 65535.0f;
    if (*humi_pct < 0) *humi_pct = 0;
    if (*humi_pct > 100) *humi_pct = 100;
    return true;
}

bool bmp280_read(float *press_hpa, float *temp_c)
{
    if (!s_bmp) return false;
    uint8_t b[6];
    if (rd_reg(ADDR_BMP280, 0xF7, b, 6) != ESP_OK) return false;
    int32_t adc_p = ((uint32_t)b[0] << 12) | ((uint32_t)b[1] << 4) | (b[2] >> 4);
    int32_t adc_t = ((uint32_t)b[3] << 12) | ((uint32_t)b[4] << 4) | (b[5] >> 4);

    /* Bosch 定点补偿 */
    int32_t v1 = ((((adc_t >> 3) - ((int32_t)cal.T1 << 1))) * cal.T2) >> 11;
    int32_t v2 = (((((adc_t >> 4) - cal.T1) * ((adc_t >> 4) - cal.T1)) >> 12)
                  * cal.T3) >> 14;
    int32_t t_fine = v1 + v2;
    *temp_c = ((t_fine * 5 + 128) >> 8) / 100.0f;

    int64_t p1 = (int64_t)t_fine - 128000;
    int64_t p2 = p1 * p1 * cal.P6 + ((p1 * cal.P5) << 17) + ((int64_t)cal.P4 << 35);
    p1 = ((p1 * p1 * cal.P3) >> 8) + ((p1 * cal.P2) << 12);
    p1 = (((((int64_t)1) << 47) + p1) * cal.P1) >> 33;
    if (p1 == 0) return false;
    int64_t p = 1048576 - adc_p;
    p = (((p << 31) - p2) * 3125) / p1;
    p1 = ((int64_t)cal.P9 * (p >> 13) * (p >> 13)) >> 25;
    p2 = ((int64_t)cal.P8 * p) >> 19;
    p = ((p + p1 + p2) >> 8) + ((int64_t)cal.P7 << 4);
    *press_hpa = (float)p / 256.0f / 100.0f; /* Pa → hPa */
    return true;
}

bool pcf8563_read(char *buf, int len)
{
    if (!s_rtc) return false;
    uint8_t b[3]; /* 秒/分/时,寄存器 0x02 起 */
    if (rd_reg(ADDR_PCF8563, 0x02, b, 3) != ESP_OK) return false;
    snprintf(buf, len, "%02d:%02d:%02d",
             bcd2dec(b[2] & 0x3F), bcd2dec(b[1] & 0x7F), bcd2dec(b[0] & 0x7F));
    return true;
}
