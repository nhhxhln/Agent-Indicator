#include "ui/i18n.h"

#include "app_state.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "i18n";
static const char *NVS_NS = "ui";
static const char *NVS_KEY = "lang";

static ui_lang_t s_lang = UI_LANG_EN;

static const char *STR_TABLE[2][STR_MAX] = {
    [UI_LANG_EN] = {
        [STR_WELCOME]          = "Agent Indicator ready\n",
        [STR_WAIT_HOST]        = "waiting for host...",
        [STR_STATE_IDLE]       = "Idle",
        [STR_STATE_CONNECTING] = "Connecting",
        [STR_STATE_THINKING]   = "Thinking",
        [STR_STATE_RESPONDING] = "Responding",
        [STR_STATE_TOOL_USE]   = "Using tool",
        [STR_STATE_WAITING]    = "Waiting for you",
        [STR_STATE_ERROR]      = "Error",
        [STR_STATE_RATELIMIT]  = "Rate limited",
        [STR_STATE_OFFLINE]    = "Offline",
        [STR_INPUT]            = "IN",
        [STR_OUTPUT]           = "OUT",
        [STR_BATTERY]          = "Battery",
    },
    [UI_LANG_ZH] = {
        [STR_WELCOME]          = "Agent Indicator 就绪\n",
        [STR_WAIT_HOST]        = "等待上位机连接...",
        [STR_STATE_IDLE]       = "空闲",
        [STR_STATE_CONNECTING] = "连接中",
        [STR_STATE_THINKING]   = "思考中",
        [STR_STATE_RESPONDING] = "回复中",
        [STR_STATE_TOOL_USE]   = "调用工具",
        [STR_STATE_WAITING]    = "等待输入",
        [STR_STATE_ERROR]      = "出错了",
        [STR_STATE_RATELIMIT]  = "限流中",
        [STR_STATE_OFFLINE]    = "离线",
        [STR_INPUT]            = "输入",
        [STR_OUTPUT]           = "输出",
        [STR_BATTERY]          = "电量",
    },
};

void ui_i18n_init(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        uint8_t v = UI_LANG_EN;
        if (nvs_get_u8(h, NVS_KEY, &v) == ESP_OK && v <= UI_LANG_ZH)
            s_lang = (ui_lang_t)v;
        nvs_close(h);
    }
    ESP_LOGI(TAG, "lang=%s", s_lang == UI_LANG_ZH ? "zh" : "en");
}

const char *tr(ui_str_id_t id)
{
    if (id >= STR_MAX) return "?";
    return STR_TABLE[s_lang][id];
}

ui_lang_t ui_lang_get(void) { return s_lang; }

esp_err_t ui_lang_set(ui_lang_t lang)
{
    if (lang > UI_LANG_ZH) return ESP_ERR_INVALID_ARG;
    s_lang = lang;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_set_u8(h, NVS_KEY, (uint8_t)lang);
        nvs_commit(h);
        nvs_close(h);
    }
    return err;
}

const char *tr_state(int agent_state)
{
    static const ui_str_id_t map[AGENT_STATE_MAX] = {
        [AGENT_IDLE]         = STR_STATE_IDLE,
        [AGENT_CONNECTING]   = STR_STATE_CONNECTING,
        [AGENT_THINKING]     = STR_STATE_THINKING,
        [AGENT_RESPONDING]   = STR_STATE_RESPONDING,
        [AGENT_TOOL_USE]     = STR_STATE_TOOL_USE,
        [AGENT_WAITING_USER] = STR_STATE_WAITING,
        [AGENT_ERROR]        = STR_STATE_ERROR,
        [AGENT_RATE_LIMITED] = STR_STATE_RATELIMIT,
        [AGENT_OFFLINE]      = STR_STATE_OFFLINE,
    };
    if (agent_state < 0 || agent_state >= AGENT_STATE_MAX) return "?";
    return tr(map[agent_state]);
}
