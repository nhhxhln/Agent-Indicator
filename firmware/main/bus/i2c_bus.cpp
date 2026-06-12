#include "i2c_bus.h"

#include "board.h"
#include "esp_check.h"
#include "esp_io_expander_tca9554.h"
#include "esp_log.h"

static const char *TAG = "i2c_bus";

static constexpr i2c_port_t kPort = I2C_NUM_0;
static esp_io_expander_handle_t s_expander = nullptr;

extern "C" esp_err_t i2c_bus_init(void)
{
    i2c_config_t cfg = {};
    cfg.mode = I2C_MODE_MASTER;
    cfg.sda_io_num = BOARD_I2C_SDA;
    cfg.scl_io_num = BOARD_I2C_SCL;
    cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.master.clk_speed = BOARD_I2C_FREQ_HZ;
    ESP_RETURN_ON_ERROR(i2c_param_config(kPort, &cfg), TAG, "param");
    ESP_RETURN_ON_ERROR(i2c_driver_install(kPort, I2C_MODE_MASTER, 0, 0, 0), TAG, "install");

    if (esp_io_expander_new_i2c_tca9554(kPort, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000,
                                        &s_expander) == ESP_OK) {
        /* P0-P5 输出(复位/使能类),P6/P7 输入(中断) */
        esp_io_expander_set_dir(s_expander,
            IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2 |
            IO_EXPANDER_PIN_NUM_3 | IO_EXPANDER_PIN_NUM_4 | IO_EXPANDER_PIN_NUM_5,
            IO_EXPANDER_OUTPUT);
        ESP_LOGI(TAG, "tca9554 ready");
    } else {
        s_expander = nullptr;
        ESP_LOGW(TAG, "tca9554 not found (bare devkit?), expander ops are no-op");
    }
    return ESP_OK;
}

extern "C" i2c_port_t i2c_bus_port(void) { return kPort; }
extern "C" esp_io_expander_handle_t io_expander(void) { return s_expander; }

extern "C" void io_expander_set(uint32_t pin_num, bool level)
{
    if (s_expander)
        esp_io_expander_set_level(s_expander, 1u << pin_num, level);
}
