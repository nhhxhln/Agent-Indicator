#include "notify.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_console.h"

#include "audio/audio.h"
#include "ui/led_engine.h"

#define NOTIFY_TONE_MAX 9   /* tone 0..9(见 audio.c kTones) */

static bool s_led = true;   /* 默认爆闪 LED */
static int  s_tone = 0;     /* 默认提示音 0;<0 = 关闭 */

void notify_set_led(bool on) { s_led = on; }

void notify_set_tone(int id)
{
    if (id < 0) s_tone = -1;
    else if (id > NOTIFY_TONE_MAX) s_tone = NOTIFY_TONE_MAX;
    else s_tone = id;
}

bool notify_get_led(void) { return s_led; }
int  notify_get_tone(void) { return s_tone; }

void notify_trigger(void)
{
    if (s_led) led_engine_flash(255, 255, 255, 3); /* 3 次白色爆闪 */
    if (s_tone >= 0) audio_play_tone((uint8_t)s_tone);
}

static int cmd_notify(int argc, char **argv)
{
    if (argc < 2) { /* 无参数 = 触发一次 */
        notify_trigger();
        printf("notify fired (led=%s, tone=%d)\n", s_led ? "on" : "off", s_tone);
        return 0;
    }
    if (strcmp(argv[1], "led") == 0) {
        if (argc >= 3) notify_set_led(strcmp(argv[2], "off") != 0);
        printf("notify led=%s\n", s_led ? "on" : "off");
        return 0;
    }
    if (strcmp(argv[1], "tone") == 0) {
        if (argc >= 3) {
            if (strcmp(argv[2], "off") == 0) notify_set_tone(-1);
            else notify_set_tone(atoi(argv[2]));
        }
        if (s_tone < 0) printf("notify tone=off\n");
        else printf("notify tone=%d\n", s_tone);
        return 0;
    }
    printf("usage: notify              - 触发一次提醒(爆闪 LED + 提示音)\n"
           "       notify led on|off   - 是否爆闪 LED\n"
           "       notify tone 0-9|off - 提示音(off=关闭)\n");
    return 1;
}

void notify_register(void)
{
    const esp_console_cmd_t c = {
        .command = "notify",
        .help = "notify [led on|off | tone 0-9|off] - 提醒(爆闪+提示音)",
        .hint = NULL,
        .func = cmd_notify,
        .argtable = NULL,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&c));
}
