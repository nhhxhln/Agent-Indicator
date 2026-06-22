/* SPI 从机:接收主机推来的逐行像素,组成整帧后上屏。
 *
 * 线协议(主机=SPI 主,模式0;每个 SPI 事务 = 一行包):
 *   [0]=0xA9 魔数  [1]=格式(0=RGB565LE,1=RGB888)  [2..3]=行号 y(LE u16)
 *   [4..] = 该行 W 个像素(565:2B/px;888:3B/px)
 *   收到 y==H-1 的行即认为一帧完整,整帧刷新到面板。
 * 收到首个合法包后 g_host_active=1,自测图案让位。 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t link_spi_start(void);

#ifdef __cplusplus
}
#endif
