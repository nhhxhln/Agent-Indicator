#include "qmi8658.hpp"

#include "board.h"
#include "bus/i2c_bus.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "qmi8658";

namespace qmi8658 {
namespace {

constexpr uint8_t REG_WHO_AM_I = 0x00; /* = 0x05 */
constexpr uint8_t REG_CTRL1 = 0x02;
constexpr uint8_t REG_CTRL2 = 0x03;    /* accel */
constexpr uint8_t REG_CTRL3 = 0x04;    /* gyro */
constexpr uint8_t REG_CTRL7 = 0x08;    /* enable */
constexpr uint8_t REG_TEMP_L = 0x33;
constexpr uint8_t REG_AX_L = 0x35;     /* ax..gz 连续 12 字节 */

constexpr float kAccLsbPerG = 4096.0f;   /* ±8g */
constexpr float kGyroLsbPerDps = 64.0f;  /* ±512dps */

bool s_present = false;

esp_err_t wr(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_write_to_device(i2c_bus_port(), I2C_ADDR_QMI8658C,
                                      buf, 2, pdMS_TO_TICKS(50));
}

esp_err_t rd(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(i2c_bus_port(), I2C_ADDR_QMI8658C,
                                        &reg, 1, data, len, pdMS_TO_TICKS(50));
}

} // namespace

esp_err_t init(void)
{
    uint8_t id = 0;
    if (rd(REG_WHO_AM_I, &id, 1) != ESP_OK || id != 0x05) {
        ESP_LOGW(TAG, "not found (who_am_i=0x%02x)", id);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_RETURN_ON_ERROR(wr(REG_CTRL1, 0x40), TAG, "ctrl1"); /* 地址自增 */
    ESP_RETURN_ON_ERROR(wr(REG_CTRL2, 0x25), TAG, "ctrl2"); /* ±8g, ~235Hz */
    ESP_RETURN_ON_ERROR(wr(REG_CTRL3, 0x55), TAG, "ctrl3"); /* ±512dps, ~235Hz */
    ESP_RETURN_ON_ERROR(wr(REG_CTRL7, 0x03), TAG, "ctrl7"); /* aEN | gEN */
    s_present = true;
    ESP_LOGI(TAG, "ready (±8g, ±512dps)");
    return ESP_OK;
}

bool present(void) { return s_present; }

esp_err_t read(Sample &out)
{
    if (!s_present) return ESP_ERR_INVALID_STATE;
    uint8_t raw[12];
    ESP_RETURN_ON_ERROR(rd(REG_AX_L, raw, sizeof(raw)), TAG, "read");
    auto s16 = [&](int i) { return (int16_t)(raw[i] | (raw[i + 1] << 8)); };
    out.ax = s16(0) / kAccLsbPerG;
    out.ay = s16(2) / kAccLsbPerG;
    out.az = s16(4) / kAccLsbPerG;
    out.gx = s16(6) / kGyroLsbPerDps;
    out.gy = s16(8) / kGyroLsbPerDps;
    out.gz = s16(10) / kGyroLsbPerDps;
    uint8_t t[2];
    if (rd(REG_TEMP_L, t, 2) == ESP_OK)
        out.temp_c = (int16_t)(t[0] | (t[1] << 8)) / 256.0f;
    return ESP_OK;
}

} // namespace qmi8658

/* C 侧(display.c 设备检测页)接口 */
extern "C" bool qmi8658_present_c(void) { return qmi8658::present(); }

extern "C" bool qmi8658_read_c(float *ax, float *ay, float *az, float *temp_c)
{
    qmi8658::Sample s;
    if (qmi8658::read(s) != ESP_OK) return false;
    *ax = s.ax; *ay = s.ay; *az = s.az; *temp_c = s.temp_c;
    return true;
}
