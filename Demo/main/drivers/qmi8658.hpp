/* QMI8658C 6 轴 IMU 最小驱动(I2C,共享总线)。 */
#pragma once

#include <cstdint>

#include "esp_err.h"

namespace qmi8658 {

struct Sample {
    float ax, ay, az;  /* g */
    float gx, gy, gz;  /* dps */
    float temp_c;
};

esp_err_t init(void);          /* 探测 WHO_AM_I 并配置 ±8g / ±512dps @~250Hz */
bool present(void);
esp_err_t read(Sample &out);

} // namespace qmi8658
