/* 共享 I2C 总线(legacy driver,port 0)+ TCA9554 IO 扩展器。
 * es8311 / esp_lcd_touch / esp_io_expander 组件均使用 legacy i2c_port_t。 */
#pragma once

#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_io_expander.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_bus_init(void);
i2c_port_t i2c_bus_port(void);
esp_io_expander_handle_t io_expander(void); /* 可能为 NULL(扩展器不在位) */
/* 置位扩展器输出;扩展器不在位时静默返回(裸开发板调试场景) */
void io_expander_set(uint32_t pin_num, bool level);
/* 地址探测(设备检测页用) */
bool i2c_probe(uint8_t addr7);

#ifdef __cplusplus
}
#endif
