/* 环境传感器:SHT4x 温湿度 / BMP280 气压 / PCF8563 RTC(共享 I2C)。
 * 全部不在位时各函数安全返回 false,Devices 页据此显示红点。 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void sensors_init(void);   /* 探测三件并初始化在位者 */

bool sht4x_present(void);
bool sht4x_read(float *temp_c, float *humi_pct);

bool bmp280_present(void);
bool bmp280_read(float *press_hpa, float *temp_c);

bool pcf8563_present(void);
bool pcf8563_read(char *buf, int len);  /* "HH:MM:SS" */

#ifdef __cplusplus
}
#endif
