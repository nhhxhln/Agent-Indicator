#include "drivers/wm8960.h"

#include "board.h"
#include "bus/i2c_bus.h"
#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "wm8960";

namespace {

constexpr uint8_t ADDR = 0x1A; /* = I2C_ADDR_WM8960 */

/* WM8960 寄存器:7-bit 地址 + 9-bit 数据,打包成 2 字节:
 *   byte0 = (reg << 1) | (data bit8),byte1 = data[7:0] */
esp_err_t wr(uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = { (uint8_t)((reg << 1) | ((val >> 8) & 0x01)),
                       (uint8_t)(val & 0xFF) };
    return i2c_master_write_to_device(i2c_bus_port(), ADDR, buf, 2, pdMS_TO_TICKS(50));
}

bool s_present = false;

} // namespace

bool wm8960_probe(void)
{
    s_present = i2c_probe(ADDR);
    return s_present;
}

esp_err_t wm8960_init(int sample_rate)
{
    if (!s_present) return ESP_ERR_NOT_FOUND;
#if BOARD_I2S_MCLK < 0
    ESP_LOGW(TAG, "BOARD_I2S_MCLK 未接,WM8960 需要 MCLK,可能无声(见 board.h)");
#endif
    (void)sample_rate; /* SYSCLK 直接取 MCLK = 256*fs,无需分频 */

    /* 标准初始化序列(16bit I2S 从机,DAC→HP+SPK,LINPUT1→ADC)。
     * 寄存器值取自 WM8960 datasheet 典型应用,⚠ 回板后核对。 */
    static const struct { uint8_t reg; uint16_t val; } SEQ[] = {
        { 0x0F, 0x0000 }, /* R15 软复位 */
        { 0x19, 0x00FE }, /* R25 PWR1: VMID=50k,VREF,AINL/R,ADCL/R,MICB */
        { 0x1A, 0x01F8 }, /* R26 PWR2: DACL/R,LOUT1,ROUT1,SPKL,SPKR */
        { 0x2F, 0x003C }, /* R47 PWR3: LMIC,RMIC,LOMIX,ROMIX */
        { 0x07, 0x0002 }, /* R7 IFACE1: I2S,16bit,从机 */
        { 0x04, 0x0000 }, /* R4 CLOCKING1: SYSCLK=MCLK 直接,DAC/ADC div=256fs */
        { 0x14, 0x01C4 }, /* R20 noise gate/ALC 旁路相关默认 */
        { 0x05, 0x0000 }, /* R5 DACCTL1: 取消静音 */
        /* ADC 输入:LINPUT1/RINPUT1 → PGA boost */
        { 0x20, 0x0138 }, /* R32 ADCL boost: LINPUT1 to PGA */
        { 0x21, 0x0138 }, /* R33 ADCR boost: RINPUT1 to PGA */
        { 0x00, 0x0117 }, /* R0 LINVOL: +0dB,update */
        { 0x01, 0x0117 }, /* R1 RINVOL */
        /* DAC 音量 0dB */
        { 0x0A, 0x01FF }, { 0x0B, 0x01FF }, /* R10/R11 DACL/R vol (+update) */
        /* 输出混音:DAC → LOMIX/ROMIX */
        { 0x22, 0x0100 }, /* R34 LOUTMIX: LD2LO 使能 */
        { 0x25, 0x0100 }, /* R37 ROUTMIX: RD2RO 使能 */
        /* 耳放音量 */
        { 0x28, 0x0179 }, { 0x29, 0x0179 }, /* R40/R41 LOUT1/ROUT1 (+update) */
        /* 喇叭 Class-D 使能;音量统一由 DAC 音量(R10/R11)+ HP(R40/R41)控制 */
        { 0x31, 0x00F7 }, /* R49 CLASSD1: SPKL+SPKR 使能 */
    };
    for (size_t i = 0; i < sizeof(SEQ) / sizeof(SEQ[0]); i++) {
        ESP_RETURN_ON_ERROR(wr(SEQ[i].reg, SEQ[i].val), TAG, "wr 0x%02x", SEQ[i].reg);
        if (SEQ[i].reg == 0x0F) vTaskDelay(pdMS_TO_TICKS(10)); /* 复位后稳定 */
    }
    ESP_LOGI(TAG, "wm8960 ready @%dHz", sample_rate);
    return ESP_OK;
}

void wm8960_set_volume(int vol)
{
    if (!s_present) return;
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    /* HP(R40/R41 LOUT1/ROUT1):0x30(-73dB)~0x7F(+6dB),0x100=update */
    uint16_t hp = 0x30 + (uint16_t)((0x7F - 0x30) * vol / 100);
    wr(0x28, 0x100 | hp);
    wr(0x29, 0x100 | hp);
    /* DAC 音量(R10/R11)同时影响喇叭输出:0xC0~0xFF */
    uint16_t dac = 0xC0 + (uint16_t)((0xFF - 0xC0) * vol / 100);
    wr(0x0A, 0x100 | dac);
    wr(0x0B, 0x100 | dac);
}
