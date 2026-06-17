/* i2c_tools 控制台命令(移植自 esp-idf i2c_tools 示例,改用 i2c_master + 共享总线):
 *   i2cdetect              扫描总线上的设备地址
 *   i2cget <addr> <reg> [len]   读寄存器
 *   i2cset <addr> <reg> <b0> [b1...]  写寄存器
 * 地址/数据均十六进制(可带 0x)。复用 bus/i2c_bus 的共享总线,无需重装驱动。 */
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bus/i2c_bus.h"
#include "console/app_console.h"
#include "esp_console.h"

namespace {

long hex(const char *s) { return strtol(s, nullptr, 16); }

int cmd_i2cdetect(int, char **)
{
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++) {
            uint8_t addr = i + j;
            if (addr < 0x08 || addr > 0x77) printf("   "); /* 保留地址 */
            else printf(i2c_probe(addr) ? "%02x " : "-- ", addr);
        }
        printf("\n");
    }
    return 0;
}

int cmd_i2cget(int argc, char **argv)
{
    if (argc < 3) { printf("usage: i2cget <addr> <reg> [len]\n"); return 1; }
    uint8_t addr = (uint8_t)hex(argv[1]);
    uint8_t reg = (uint8_t)hex(argv[2]);
    int len = argc >= 4 ? atoi(argv[3]) : 1;
    if (len < 1 || len > 32) len = 1;
    uint8_t buf[32];
    if (i2c_bus_read_reg(addr, reg, buf, len) != ESP_OK) {
        printf("read failed (addr 0x%02x not present?)\n", addr);
        return 1;
    }
    printf("0x%02x[0x%02x]:", addr, reg);
    for (int i = 0; i < len; i++) printf(" %02x", buf[i]);
    printf("\n");
    return 0;
}

int cmd_i2cset(int argc, char **argv)
{
    if (argc < 4) { printf("usage: i2cset <addr> <reg> <b0> [b1 ...]\n"); return 1; }
    uint8_t addr = (uint8_t)hex(argv[1]);
    uint8_t buf[16];
    int n = 0;
    buf[n++] = (uint8_t)hex(argv[2]);            /* reg */
    for (int i = 3; i < argc && n < 16; i++) buf[n++] = (uint8_t)hex(argv[i]);
    if (i2c_bus_write(addr, buf, n) != ESP_OK) {
        printf("write failed (addr 0x%02x not present?)\n", addr);
        return 1;
    }
    printf("ok\n");
    return 0;
}

} // namespace

extern "C" void case_i2c_register(void)
{
    const esp_console_cmd_t cmds[] = {
        { "i2cdetect", "扫描 I2C 总线设备地址", nullptr, cmd_i2cdetect, nullptr },
        { "i2cget", "i2cget <addr> <reg> [len] - 读寄存器(hex)", nullptr, cmd_i2cget, nullptr },
        { "i2cset", "i2cset <addr> <reg> <b0...> - 写寄存器(hex)", nullptr, cmd_i2cset, nullptr },
    };
    for (auto &c : cmds) ESP_ERROR_CHECK(esp_console_cmd_register(&c));
}
