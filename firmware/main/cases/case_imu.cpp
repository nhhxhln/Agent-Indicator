/* IMU 可视化 case:
 *   imu              单次读取并打印 6 轴 + 温度
 *   imu_vis [on|off] 持续可视化:屏幕在位时 LVGL 页面(roll/pitch 水平仪 +
 *                    6 轴条形图),否则降级为 5Hz 日志输出
 */
#include <cmath>
#include <cstdio>
#include <cstring>

#include "console/app_console.h"
#include "drivers/qmi8658.hpp"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ui/display.h"

static const char *TAG = "case_imu";

namespace {

constexpr int kVisStack = 6144; /* LVGL 调用 + float printf */

TaskHandle_t s_vis_task = nullptr;
lv_obj_t *s_screen = nullptr, *s_prev_screen = nullptr;
lv_obj_t *s_bars[6];
lv_obj_t *s_attitude_label;
lv_obj_t *s_bubble; /* 水平仪气泡 */

void vis_ui_create(void)
{
    lvgl_port_lock(0);
    s_prev_screen = lv_screen_active();
    s_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x101018), 0);

    static const char *names[6] = { "AX", "AY", "AZ", "GX", "GY", "GZ" };
    for (int i = 0; i < 6; i++) {
        lv_obj_t *lbl = lv_label_create(s_screen);
        lv_label_set_text(lbl, names[i]);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x8888aa), 0);
        lv_obj_set_pos(lbl, 20, 250 + i * 36);
        s_bars[i] = lv_bar_create(s_screen);
        lv_bar_set_range(s_bars[i], -100, 100);
        lv_obj_set_size(s_bars[i], 380, 18);
        lv_obj_set_pos(s_bars[i], 70, 252 + i * 36);
    }
    /* 水平仪:外圈 + 气泡 */
    lv_obj_t *circle = lv_obj_create(s_screen);
    lv_obj_set_size(circle, 180, 180);
    lv_obj_set_pos(circle, 150, 40);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, lv_color_hex(0x181828), 0);
    s_bubble = lv_obj_create(s_screen);
    lv_obj_set_size(s_bubble, 24, 24);
    lv_obj_set_style_radius(s_bubble, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_bubble, lv_color_hex(0x00cc88), 0);
    s_attitude_label = lv_label_create(s_screen);
    lv_obj_set_pos(s_attitude_label, 20, 10);
    lv_obj_set_style_text_color(s_attitude_label, lv_color_hex(0xeeeeee), 0);
    lv_screen_load(s_screen);
    lvgl_port_unlock();
}

void vis_ui_destroy(void)
{
    if (!s_screen) return;
    lvgl_port_lock(0);
    if (s_prev_screen) lv_screen_load(s_prev_screen);
    lv_obj_delete(s_screen);
    s_screen = nullptr;
    lvgl_port_unlock();
}

void vis_task(void *)
{
    const bool gui = display_ready();
    if (gui) vis_ui_create();
    int log_div = 0;
    while (true) {
        qmi8658::Sample s;
        if (qmi8658::read(s) == ESP_OK) {
            float roll = atan2f(s.ay, s.az) * 180.0f / (float)M_PI;
            float pitch = atan2f(-s.ax, sqrtf(s.ay * s.ay + s.az * s.az))
                          * 180.0f / (float)M_PI;
            if (gui) {
                lvgl_port_lock(0);
                lv_bar_set_value(s_bars[0], (int)(s.ax * 50), LV_ANIM_OFF);
                lv_bar_set_value(s_bars[1], (int)(s.ay * 50), LV_ANIM_OFF);
                lv_bar_set_value(s_bars[2], (int)(s.az * 50), LV_ANIM_OFF);
                lv_bar_set_value(s_bars[3], (int)(s.gx / 5), LV_ANIM_OFF);
                lv_bar_set_value(s_bars[4], (int)(s.gy / 5), LV_ANIM_OFF);
                lv_bar_set_value(s_bars[5], (int)(s.gz / 5), LV_ANIM_OFF);
                lv_obj_set_pos(s_bubble, 228 + (int)(pitch * 1.5f),
                               118 + (int)(roll * 1.5f));
                lv_label_set_text_fmt(s_attitude_label,
                                      "roll %.1f  pitch %.1f  %.1fC",
                                      (double)roll, (double)pitch, (double)s.temp_c);
                lvgl_port_unlock();
            } else if (++log_div >= 10) { /* 无屏:5Hz 日志 */
                log_div = 0;
                ESP_LOGI(TAG, "a=[%+.2f %+.2f %+.2f]g g=[%+6.1f %+6.1f %+6.1f]dps "
                              "roll=%+.1f pitch=%+.1f",
                         (double)s.ax, (double)s.ay, (double)s.az,
                         (double)s.gx, (double)s.gy, (double)s.gz,
                         (double)roll, (double)pitch);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); /* 50Hz */
    }
}

int cmd_imu(int, char **)
{
    qmi8658::Sample s;
    if (qmi8658::read(s) != ESP_OK) {
        printf("imu not present\n");
        return 1;
    }
    printf("accel %+.3f %+.3f %+.3f g | gyro %+7.2f %+7.2f %+7.2f dps | %.1f C\n",
           (double)s.ax, (double)s.ay, (double)s.az,
           (double)s.gx, (double)s.gy, (double)s.gz, (double)s.temp_c);
    return 0;
}

int cmd_imu_vis(int argc, char **argv)
{
    bool on = argc < 2 || strcmp(argv[1], "on") == 0;
    if (on && !s_vis_task) {
        if (!qmi8658::present()) { printf("imu not present\n"); return 1; }
        xTaskCreate(vis_task, "imu_vis", kVisStack, nullptr, 5, &s_vis_task);
        printf("imu_vis on (%s)\n", display_ready() ? "lcd" : "log");
    } else if (!on && s_vis_task) {
        vTaskDelete(s_vis_task);
        s_vis_task = nullptr;
        vis_ui_destroy();
        printf("imu_vis off\n");
    }
    return 0;
}

} // namespace

extern "C" void case_imu_register(void)
{
    qmi8658::init(); /* 不在位仅警告,命令仍注册 */
    const esp_console_cmd_t cmds[] = {
        { "imu", "单次读取 6 轴 + 温度", nullptr, cmd_imu, nullptr },
        { "imu_vis", "imu_vis [on|off] - 持续可视化(LCD/日志)", nullptr, cmd_imu_vis, nullptr },
    };
    for (auto &c : cmds) ESP_ERROR_CHECK(esp_console_cmd_register(&c));
}
