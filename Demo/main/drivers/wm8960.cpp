#include "drivers/wm8960.h"

#include "board.h"
#include "bus/i2c_bus.h"
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
    return i2c_bus_write(ADDR, buf, 2);
}

bool s_present = false;

} // namespace

bool wm8960_probe(void)
{
    s_present = i2c_probe(ADDR);
    return s_present;
}

esp_err_t wm8960_write_reg(uint8_t reg, uint16_t val) { return wr(reg, val); }

esp_err_t wm8960_init(int sample_rate)
{
    if (!s_present) return ESP_ERR_NOT_FOUND;
#if BOARD_I2S_MCLK < 0
    ESP_LOGW(TAG, "BOARD_I2S_MCLK 未接,WM8960 需要 MCLK,可能无声(见 board.h)");
#endif
    (void)sample_rate; /* SYSCLK 直接取 MCLK = 256*fs,无需分频 */

    /* 标准初始化序列(16bit I2S 从机,DAC→HP(LOUT1/ROUT1)+SPK,LINPUT1→ADC)。
     * 寄存器分配与时序对照 Waveshare WM8960 STM32 参考工程
     * (WM8960_Audio_Board_Code/Open429I_I2S/.../WM8960.c)核对。 */
    static const struct { uint8_t reg; uint16_t val; } SEQ[] = {
        { 0x0F, 0xFFFF }, /* R15 软复位(任意值触发) */
        { 0x19, 0x00FE }, /* R25 PWR1: VMID=50k,VREF,AINL/R,ADCL/R,MICB(录放都开) */
        { 0x1A, 0x01F9 }, /* R26 PWR2: DACL/R,LOUT1,ROUT1,SPKL,SPKR + PLLEN(bit0) */
        { 0x2F, 0x003C }, /* R47 PWR3: LMIC,RMIC,LOMIX,ROMIX */
        /* ★ 时钟来自板载 24MHz 晶振(MCLK 不来自 ESP32!),必须用内部 PLL 锁出
         * 12.288MHz SYSCLK 才能跑 48k。24MHz→12.288M:f2=49.152M×... N=8,
         * pre_div=1,K=0x3126E9(与 Linux 内核 wm8960 pll_factors 算法一致)。
         * 之前 R4=0 把 SYSCLK 当 24MHz,DAC 实际跑 93.75kHz → 全是破音的真因。 */
        { 0x34, 0x0038 }, /* R52 PLL1: SDM=1,PLLPRESCALE=1,PLLN=8 */
        { 0x35, 0x0031 }, /* R53 PLL2: K[23:16]=0x31 */
        { 0x36, 0x0026 }, /* R54 PLL3: K[15:8]=0x26 */
        { 0x37, 0x00E9 }, /* R55 PLL4: K[7:0]=0xE9(写完等 PLL 锁定) */
        { 0x04, 0x0005 }, /* R4 CLOCKING1: CLKSEL=PLL,SYSCLKDIV=÷2,DAC/ADC=SYSCLK/256→48k */
        { 0x08, 0x01C0 }, /* R8 CLOCKING2: DCLKDIV=÷16 → 12.288M/16=768kHz(Class-D 理想) */
        { 0x05, 0x0000 }, /* R5 DACCTL1: 取消静音(DAC 不能 mute) */
        { 0x07, 0x0002 }, /* R7 IFACE1: I2S,16bit,从机 */
        /* DAC 数字音量 0dB */
        { 0x0A, 0x01FF }, { 0x0B, 0x01FF }, /* R10/R11 DACL/R vol (+update) */
        /* 输出混音:DAC → LOMIX/ROMIX */
        { 0x22, 0x0100 }, /* R34 LOUTMIX: LD2LO(左 DAC→左输出混音) */
        { 0x25, 0x0100 }, /* R37 ROUTMIX: RD2RO(右 DAC→右输出混音) */
        /* 耳机输出音量 LOUT1/ROUT1 = R2/R3 */
        { 0x02, 0x016F }, /* R2 LOUT1VOL: 0x6F + update(bit8) */
        { 0x03, 0x016F }, /* R3 ROUT1VOL */
        /* 喇叭音量 SPK_L/SPK_R = R40/R41(0x28/0x29) */
        { 0x28, 0x017F }, /* R40 SPKLVOL: +6dB + update */
        { 0x29, 0x017F }, /* R41 SPKRVOL */
        { 0x31, 0x00F7 }, /* R49 CLASSD1: SPKL+SPKR Class-D 使能 */
        /* ---- ADC 录音通路:LINPUT1/RINPUT1 → PGA → ADC ----
         * ⚠ MIC BOOST 保持 00(不加 +13/+20/+29dB),之前 0x138 开 +29dB
         * 在悬空/无咪头时自激造成失真。需要更大灵敏度再调 R32/R33 bit5:4。 */
        { 0x20, 0x0108 }, /* R32 ADCL path: LMN1(LINPUT1→PGA) + LMIC2B,boost=0dB */
        { 0x21, 0x0000 }, /* R33 ADCR path: 关闭右输入(单咪头在左,避免右路悬空噪声被回灌) */
        { 0x00, 0x0117 }, /* R0 LINVOL: PGA 0dB + update */
        { 0x01, 0x0117 }, /* R1 RINVOL: PGA 0dB + update */
        { 0x15, 0x01C3 }, /* R21 ADCL 数字音量 0dB + update */
        { 0x16, 0x01C3 }, /* R22 ADCR 数字音量 0dB + update */
    };
    for (size_t i = 0; i < sizeof(SEQ) / sizeof(SEQ[0]); i++) {
        ESP_RETURN_ON_ERROR(wr(SEQ[i].reg, SEQ[i].val), TAG, "wr 0x%02x", SEQ[i].reg);
        if (SEQ[i].reg == 0x0F) vTaskDelay(pdMS_TO_TICKS(10)); /* 复位后稳定 */
        if (SEQ[i].reg == 0x37) vTaskDelay(pdMS_TO_TICKS(10)); /* 等 PLL 锁定再切 CLKSEL */
    }
    ESP_LOGI(TAG, "wm8960 ready @%dHz", sample_rate);
    return ESP_OK;
}

void wm8960_set_volume(int vol)
{
    if (!s_present) return;
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    /* DAC 数字音量保持 init 时的 0dB(R10/R11),只调模拟输出,避免双重衰减。
     * 模拟输出范围 0x30(-73dB,近静音)~0x7F(+6dB);0x30 以下为静音区。
     * 把 1..100 映射到实用区间 0x60(-25dB)~0x7F(+6dB),vol=0 才真正静音,
     * 这样 vol=10 也仍听得见(之前线性到 0x37 ≈ -66dB,等于没声)。 */
    uint16_t a = (vol == 0) ? 0x30
                            : (uint16_t)(0x60 + (0x7F - 0x60) * vol / 100);
    wr(0x02, 0x100 | a); /* R2 耳机 LOUT1 */
    wr(0x03, 0x100 | a); /* R3 耳机 ROUT1 */
    wr(0x28, 0x100 | a); /* R40 喇叭 SPKL */
    wr(0x29, 0x100 | a); /* R41 喇叭 SPKR */
}
