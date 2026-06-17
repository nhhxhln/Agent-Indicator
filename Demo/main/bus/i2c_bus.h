/* 共享 I2C 总线(新版 i2c_master 驱动,port 0)+ TCA9554 IO 扩展器。
 * Demo 全面迁移到 driver/i2c_master.h(legacy driver/i2c.h 已弃用并告警)。
 * touch / tca9554 组件均用 i2c_master_bus_handle_t(v2)。 */
#pragma once

#include <stddef.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_io_expander.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_bus_init(void);
i2c_master_bus_handle_t i2c_bus_handle(void);     /* 给 touch / tca9554 等组件 */
esp_io_expander_handle_t io_expander(void);        /* 可能为 NULL(扩展器不在位) */
void io_expander_set(uint32_t pin_num, bool level);
bool i2c_probe(uint8_t addr7);                     /* i2c_master_probe */

/* 便捷读写(内部按 7-bit 地址缓存 device handle) */
esp_err_t i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len);
esp_err_t i2c_bus_read(uint8_t addr, uint8_t *data, size_t len);
esp_err_t i2c_bus_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
esp_err_t i2c_bus_wr_rd(uint8_t addr, const uint8_t *w, size_t wn,
                        uint8_t *r, size_t rn);

#ifdef __cplusplus
}
#endif
