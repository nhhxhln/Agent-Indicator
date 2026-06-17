/* 测试控制台:USB Serial/JTAG REPL,各 case 在此注册命令。
 * 注意:REPL 与 TinyUSB vendor EP 互斥(同一 USB PHY),量产固件二选一。 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_console_start(void); /* 内部依次调用各 case 的 register */

/* 各测试 case 的命令注册入口(实现在 cases 目录;audio 在 audio.cpp) */
void case_can_register(void);
void case_audio_register(void);
void case_imu_register(void);
void case_storage_register(void);
void case_i2c_register(void);

#ifdef __cplusplus
}
#endif
