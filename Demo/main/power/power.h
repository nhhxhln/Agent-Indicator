/* 电源遥测:MP2760(充电状态)+ INA226(VSYS 电流)+ 电池分压 ADC。 */
#pragma once

#include "esp_err.h"

esp_err_t power_start(void); /* 周期采样并经 comm_send 上报 TELEMETRY */
