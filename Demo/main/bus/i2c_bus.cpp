#include "i2c_bus.h"

#include "board.h"
#include "esp_check.h"
#include "esp_io_expander_tca9554.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "i2c_bus";

static i2c_master_bus_handle_t s_bus = nullptr;
static esp_io_expander_handle_t s_expander = nullptr;

/* 设备 handle 缓存(按 7-bit 地址) */
static struct {
    uint8_t addr;
    i2c_master_dev_handle_t dev;
} s_devs[16];
static int s_ndev = 0;

static i2c_master_dev_handle_t dev_for(uint8_t addr)
{
    for (int i = 0; i < s_ndev; i++)
        if (s_devs[i].addr == addr) return s_devs[i].dev;
    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = addr;
    cfg.scl_speed_hz = BOARD_I2C_FREQ_HZ;
    i2c_master_dev_handle_t dev = nullptr;
    if (i2c_master_bus_add_device(s_bus, &cfg, &dev) != ESP_OK) return nullptr;
    if (s_ndev < 16) { s_devs[s_ndev].addr = addr; s_devs[s_ndev].dev = dev; s_ndev++; }
    return dev;
}

extern "C" esp_err_t i2c_bus_init(void)
{
    i2c_master_bus_config_t bc = {};
    bc.i2c_port = I2C_NUM_0;
    bc.sda_io_num = (gpio_num_t)BOARD_I2C_SDA;
    bc.scl_io_num = (gpio_num_t)BOARD_I2C_SCL;
    bc.clk_source = I2C_CLK_SRC_DEFAULT;
    bc.glitch_ignore_cnt = 7;
    bc.flags.enable_internal_pullup = true;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bc, &s_bus), TAG, "new bus");

    /* TCA9554(v2,接受 bus handle);不在位则 io_expander_set 自动空操作 */
    if (esp_io_expander_new_i2c_tca9554(s_bus, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000,
                                        &s_expander) == ESP_OK) {
        esp_io_expander_set_dir(s_expander,
            IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2 |
            IO_EXPANDER_PIN_NUM_3 | IO_EXPANDER_PIN_NUM_4 | IO_EXPANDER_PIN_NUM_5,
            IO_EXPANDER_OUTPUT);
        ESP_LOGI(TAG, "tca9554 ready");
    } else {
        s_expander = nullptr;
        ESP_LOGW(TAG, "tca9554 not found, expander ops are no-op");
    }
    return ESP_OK;
}

extern "C" i2c_master_bus_handle_t i2c_bus_handle(void) { return s_bus; }
extern "C" esp_io_expander_handle_t io_expander(void) { return s_expander; }

extern "C" void io_expander_set(uint32_t pin_num, bool level)
{
    if (s_expander)
        esp_io_expander_set_level(s_expander, 1u << pin_num, level);
}

extern "C" bool i2c_probe(uint8_t addr7)
{
    return s_bus && i2c_master_probe(s_bus, addr7, 50) == ESP_OK;
}

extern "C" esp_err_t i2c_bus_write(uint8_t addr, const uint8_t *data, size_t len)
{
    i2c_master_dev_handle_t dev = dev_for(addr);
    return dev ? i2c_master_transmit(dev, data, len, 100) : ESP_ERR_INVALID_STATE;
}

extern "C" esp_err_t i2c_bus_read(uint8_t addr, uint8_t *data, size_t len)
{
    i2c_master_dev_handle_t dev = dev_for(addr);
    return dev ? i2c_master_receive(dev, data, len, 100) : ESP_ERR_INVALID_STATE;
}

extern "C" esp_err_t i2c_bus_wr_rd(uint8_t addr, const uint8_t *w, size_t wn,
                                   uint8_t *r, size_t rn)
{
    i2c_master_dev_handle_t dev = dev_for(addr);
    return dev ? i2c_master_transmit_receive(dev, w, wn, r, rn, 100)
               : ESP_ERR_INVALID_STATE;
}

extern "C" esp_err_t i2c_bus_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_bus_wr_rd(addr, &reg, 1, data, len);
}
