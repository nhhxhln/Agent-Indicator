/* 提醒/通知:一键触发"爆闪 LED + 提示音",并可单独开关两者。
 * 适合从 CLI 或代码里调用提醒(如长任务完成、收到消息)。 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void notify_trigger(void);       /* 按当前配置触发一次(爆闪 + 提示音) */
void notify_set_led(bool on);    /* 是否爆闪 LED */
void notify_set_tone(int id);    /* 0..9 选择并启用提示音;<0 关闭 */
bool notify_get_led(void);
int  notify_get_tone(void);      /* <0 表示关闭 */
void notify_register(void);      /* 注册 `notify` 控制台命令 */

#ifdef __cplusplus
}
#endif
