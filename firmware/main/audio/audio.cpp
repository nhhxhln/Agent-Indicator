/* 音频子系统(C++):ES8311 + I2S 全双工 16kHz/16bit 单声道。
 *
 * 架构:单一 capture 任务独占 I2S RX —— 永远计算 VU(g_app.mic_level_db),
 * 并按当前模式把同一份数据分发给 录音(RAM/SD)/ loopback;TX 侧由互斥锁
 * 串行化(tone / player / loopback 不并发)。避免多任务争抢 I2S 句柄。
 *
 * 控制台 case(cases 注册在本文件,见 audio_register_cases):
 *   audio_rec [sec]        录音到 PSRAM(默认 5s,上限 30s)
 *   audio_play             回放 PSRAM 录音(rec/replay)
 *   audio_rec_sd <f> [sec] 录音到 /sdcard/<f>.wav(rec_to_sd)
 *   audio_player <path>    播放 WAV,/sdcard 或 /spiffs(player_from_sd)
 *   audio_loop [on|off]    MIC→SPK 直通(loopback)
 *   audio_vol <0-100>      DAC 音量
 *   tone <0-3>             提示音
 */
#include "audio/audio.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app_state.h"
#include "board.h"
#include "bus/i2c_bus.h"
#include "console/app_console.h"
#include "driver/i2s_std.h"
#include "es8311.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "storage/storage.h"

static const char *TAG = "audio";

namespace {

constexpr int kSampleRate = 16000;
constexpr int kChunkSamples = 320;            /* 20ms */
constexpr int kRecMaxSec = 30;
constexpr int kCaptureStack = 6144;           /* 文件 IO + log */
constexpr int kPlayerStack = 8192;            /* vfs + 解析 + log */
constexpr int kToneStack = 4096;

i2s_chan_handle_t s_tx, s_rx;
es8311_handle_t s_codec;
bool s_ready = false;

SemaphoreHandle_t s_tx_mtx;                   /* tone/player/loopback 串行化 */
QueueHandle_t s_tone_q;

enum class Capture { kNone, kRecRam, kRecSd, kLoopback };
volatile Capture s_cap = Capture::kNone;
int16_t *s_ram_buf = nullptr;                 /* PSRAM 录音缓冲 */
size_t s_ram_len = 0, s_ram_cap = 0;          /* 单位:样本 */
FILE *s_rec_file = nullptr;
size_t s_rec_left = 0;                        /* 剩余样本数 */

/* ---------------------------------------------------------------- WAV */

struct __attribute__((packed)) WavHeader {
    char riff[4]; uint32_t size; char wave[4];
    char fmt[4]; uint32_t fmt_size; uint16_t format; uint16_t channels;
    uint32_t rate; uint32_t byte_rate; uint16_t align; uint16_t bits;
    char data[4]; uint32_t data_size;
};

WavHeader wav_header(uint32_t data_bytes, uint32_t rate, uint16_t ch)
{
    WavHeader h = {};
    memcpy(h.riff, "RIFF", 4); memcpy(h.wave, "WAVE", 4);
    memcpy(h.fmt, "fmt ", 4); memcpy(h.data, "data", 4);
    h.size = data_bytes + sizeof(WavHeader) - 8;
    h.fmt_size = 16; h.format = 1; h.channels = ch; h.rate = rate;
    h.bits = 16; h.align = ch * 2; h.byte_rate = rate * h.align;
    h.data_size = data_bytes;
    return h;
}

/* ---------------------------------------------------------------- TX */

void pa_enable(bool on) { io_expander_set(EXP_PA_EN, on); }

void play_pcm(const int16_t *pcm, size_t samples)
{
    if (!s_ready) return;
    xSemaphoreTake(s_tx_mtx, portMAX_DELAY);
    pa_enable(true);
    size_t written;
    i2s_channel_write(s_tx, pcm, samples * 2, &written, portMAX_DELAY);
    pa_enable(false);
    xSemaphoreGive(s_tx_mtx);
}

/* ------------------------------------------------------------- capture */

void capture_task(void *)
{
    static int16_t chunk[kChunkSamples];
    while (true) {
        if (!s_ready) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        size_t got = 0;
        if (i2s_channel_read(s_rx, chunk, sizeof(chunk), &got, pdMS_TO_TICKS(100)) != ESP_OK)
            continue;
        size_t n = got / 2;

        /* VU:RMS → dB,映射 0..60 给拾音条 */
        uint64_t acc = 0;
        for (size_t i = 0; i < n; i++) acc += (int32_t)chunk[i] * chunk[i];
        float rms = sqrtf((float)acc / (n ? n : 1));
        float db = 20.0f * log10f(rms / 32768.0f) + 90.0f; /* 噪声底 ~30 */
        g_app.mic_level_db = (uint8_t)fmaxf(0, fminf(60, db - 30));

        switch (s_cap) {
        case Capture::kRecRam: {
            size_t take = n < s_ram_cap - s_ram_len ? n : s_ram_cap - s_ram_len;
            memcpy(s_ram_buf + s_ram_len, chunk, take * 2);
            s_ram_len += take;
            if (s_ram_len >= s_ram_cap) {
                s_cap = Capture::kNone;
                ESP_LOGI(TAG, "rec done: %u samples", (unsigned)s_ram_len);
            }
            break;
        }
        case Capture::kRecSd: {
            size_t take = n < s_rec_left ? n : s_rec_left;
            fwrite(chunk, 2, take, s_rec_file);
            s_rec_left -= take;
            if (s_rec_left == 0) {
                long bytes = ftell(s_rec_file) - sizeof(WavHeader);
                WavHeader h = wav_header(bytes, kSampleRate, 1);
                fseek(s_rec_file, 0, SEEK_SET);
                fwrite(&h, sizeof(h), 1, s_rec_file);
                fclose(s_rec_file);
                s_rec_file = nullptr;
                s_cap = Capture::kNone;
                ESP_LOGI(TAG, "rec_sd done: %ld bytes", bytes);
            }
            break;
        }
        case Capture::kLoopback: {
            size_t w;
            i2s_channel_write(s_tx, chunk, n * 2, &w, 0); /* 不阻塞,欠载就丢 */
            break;
        }
        default:
            break;
        }
    }
}

/* ---------------------------------------------------------------- tone */

struct ToneNote { int freq; int ms; };
const ToneNote kTones[4][3] = {
    { { 880, 90 }, { 1320, 140 }, { 0, 0 } },   /* 0 done */
    { { 1047, 180 }, { 0, 0 }, { 0, 0 } },      /* 1 attention */
    { { 220, 250 }, { 196, 250 }, { 0, 0 } },   /* 2 error */
    { { 523, 80 }, { 659, 80 }, { 784, 120 } }, /* 3 boot */
};

void tone_task(void *)
{
    static int16_t buf[kSampleRate / 2]; /* 最长 500ms 音符 */
    uint8_t id;
    while (true) {
        if (xQueueReceive(s_tone_q, &id, portMAX_DELAY) != pdTRUE) continue;
        if (id >= 4) continue;
        for (const auto &note : kTones[id]) {
            if (note.freq == 0) break;
            int n = kSampleRate * note.ms / 1000;
            for (int i = 0; i < n; i++) {
                float env = i < n / 8 ? (float)i / (n / 8)
                          : i > n * 7 / 8 ? (float)(n - i) / (n / 8) : 1.0f;
                buf[i] = (int16_t)(8000 * env *
                                   sinf(2 * (float)M_PI * note.freq * i / kSampleRate));
            }
            play_pcm(buf, n);
        }
    }
}

/* ------------------------------------------------------------ player */

char s_player_path[128];

void player_task(void *)
{
    FILE *f = fopen(s_player_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "open %s failed", s_player_path);
        vTaskDelete(nullptr);
        return;
    }
    WavHeader h;
    if (fread(&h, sizeof(h), 1, f) != 1 || memcmp(h.riff, "RIFF", 4) != 0 ||
        h.format != 1 || h.bits != 16) {
        ESP_LOGE(TAG, "not a PCM16 wav");
        fclose(f);
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(TAG, "wav: %luHz %uch", (unsigned long)h.rate, h.channels);
    if (h.rate != kSampleRate)
        ESP_LOGW(TAG, "rate != %d,播放变速(重采样 TODO)", kSampleRate);

    static int16_t io_buf[2048];
    xSemaphoreTake(s_tx_mtx, portMAX_DELAY);
    pa_enable(true);
    size_t n;
    while ((n = fread(io_buf, 2, 2048, f)) > 0) {
        if (h.channels == 2) { /* 取左声道下混 */
            for (size_t i = 0; i < n / 2; i++) io_buf[i] = io_buf[i * 2];
            n /= 2;
        }
        size_t w;
        i2s_channel_write(s_tx, io_buf, n * 2, &w, portMAX_DELAY);
    }
    pa_enable(false);
    xSemaphoreGive(s_tx_mtx);
    fclose(f);
    ESP_LOGI(TAG, "player done");
    vTaskDelete(nullptr);
}

/* ------------------------------------------------------- console cases */

int cmd_rec(int argc, char **argv)
{
    if (s_cap != Capture::kNone) { printf("busy\n"); return 1; }
    int sec = argc >= 2 ? atoi(argv[1]) : 5;
    if (sec < 1 || sec > kRecMaxSec) sec = 5;
    size_t cap = (size_t)kSampleRate * sec;
    if (!s_ram_buf || s_ram_cap < cap) {
        free(s_ram_buf);
        s_ram_buf = (int16_t *)heap_caps_malloc(cap * 2, MALLOC_CAP_SPIRAM);
        if (!s_ram_buf) { printf("psram alloc failed\n"); s_ram_cap = 0; return 1; }
    }
    s_ram_cap = cap;
    s_ram_len = 0;
    s_cap = Capture::kRecRam;
    printf("recording %ds to ram...\n", sec);
    return 0;
}

int cmd_play(int, char **)
{
    if (s_ram_len == 0) { printf("nothing recorded\n"); return 1; }
    printf("replaying %u samples...\n", (unsigned)s_ram_len);
    play_pcm(s_ram_buf, s_ram_len); /* 阻塞在 REPL 任务,录音 ≤30s 可接受 */
    return 0;
}

int cmd_rec_sd(int argc, char **argv)
{
    if (s_cap != Capture::kNone) { printf("busy\n"); return 1; }
    if (argc < 2) { printf("usage: audio_rec_sd <name> [sec]\n"); return 1; }
    if (!storage_sd_mounted()) { printf("sd not mounted\n"); return 1; }
    int sec = argc >= 3 ? atoi(argv[2]) : 5;
    char path[96];
    snprintf(path, sizeof(path), STORAGE_SD_BASE "/%s.wav", argv[1]);
    s_rec_file = fopen(path, "wb");
    if (!s_rec_file) { printf("open %s failed\n", path); return 1; }
    WavHeader h = wav_header(0, kSampleRate, 1); /* 占位,结束时回填 */
    fwrite(&h, sizeof(h), 1, s_rec_file);
    s_rec_left = (size_t)kSampleRate * sec;
    s_cap = Capture::kRecSd;
    printf("recording %ds to %s...\n", sec, path);
    return 0;
}

int cmd_player(int argc, char **argv)
{
    if (argc < 2) { printf("usage: audio_player /sdcard/x.wav\n"); return 1; }
    strlcpy(s_player_path, argv[1], sizeof(s_player_path));
    xTaskCreate(player_task, "player", kPlayerStack, nullptr, 7, nullptr);
    return 0;
}

int cmd_loop(int argc, char **argv)
{
    bool on = argc < 2 || strcmp(argv[1], "on") == 0;
    if (on) {
        if (s_cap != Capture::kNone) { printf("busy\n"); return 1; }
        xSemaphoreTake(s_tx_mtx, portMAX_DELAY); /* 占住 TX 直到 off */
        pa_enable(true);
        s_cap = Capture::kLoopback;
        printf("loopback on\n");
    } else if (s_cap == Capture::kLoopback) {
        s_cap = Capture::kNone;
        pa_enable(false);
        xSemaphoreGive(s_tx_mtx);
        printf("loopback off\n");
    }
    return 0;
}

int cmd_vol(int argc, char **argv)
{
    if (argc < 2 || !s_codec) return 1;
    int set = 0;
    es8311_voice_volume_set(s_codec, atoi(argv[1]), &set);
    printf("volume=%d\n", set);
    return 0;
}

int cmd_tone(int argc, char **argv)
{
    audio_play_tone(argc >= 2 ? atoi(argv[1]) : 0);
    return 0;
}

} // namespace

extern "C" void case_audio_register(void)
{
    const esp_console_cmd_t cmds[] = {
        { "audio_rec", "audio_rec [sec] - 录音到 PSRAM", nullptr, cmd_rec, nullptr },
        { "audio_play", "回放 PSRAM 录音", nullptr, cmd_play, nullptr },
        { "audio_rec_sd", "audio_rec_sd <name> [sec] - 录音到 SD", nullptr, cmd_rec_sd, nullptr },
        { "audio_player", "audio_player <path> - 播放 WAV", nullptr, cmd_player, nullptr },
        { "audio_loop", "audio_loop [on|off] - MIC->SPK 直通", nullptr, cmd_loop, nullptr },
        { "audio_vol", "audio_vol <0-100>", nullptr, cmd_vol, nullptr },
        { "tone", "tone <0-3> - 提示音", nullptr, cmd_tone, nullptr },
    };
    for (auto &c : cmds) ESP_ERROR_CHECK(esp_console_cmd_register(&c));
}

extern "C" void audio_play_tone(uint8_t tone_id)
{
    if (s_tone_q) xQueueSend(s_tone_q, &tone_id, 0);
}

extern "C" esp_err_t audio_start(void)
{
    s_tx_mtx = xSemaphoreCreateMutex();
    s_tone_q = xQueueCreate(4, sizeof(uint8_t));

    /* I2S 全双工:BCLK 作 codec 时钟源,无 MCLK 线(docs/01 §4.3) */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx, &s_rx), TAG, "chan");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)BOARD_I2S_BCLK,
            .ws   = (gpio_num_t)BOARD_I2S_WS,
            .dout = (gpio_num_t)BOARD_I2S_DOUT,
            .din  = (gpio_num_t)BOARD_I2S_DIN,
            .invert_flags = {},
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &std_cfg), TAG, "tx std");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx, &std_cfg), TAG, "rx std");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx), TAG, "tx en");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx), TAG, "rx en");

    s_codec = es8311_create(i2c_bus_port(), ES8311_ADDRRES_0);
    if (s_codec) {
        es8311_clock_config_t clk = {
            .mclk_inverted = false,
            .sclk_inverted = false,
            .mclk_from_mclk_pin = false, /* SCLK 作 MCLK,省 GPIO */
            .mclk_frequency = 0,
            .sample_frequency = kSampleRate,
        };
        if (es8311_init(s_codec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) == ESP_OK) {
            es8311_voice_volume_set(s_codec, 70, nullptr);
            es8311_microphone_config(s_codec, false);
            es8311_microphone_gain_set(s_codec, ES8311_MIC_GAIN_18DB);
            s_ready = true;
            ESP_LOGI(TAG, "es8311 ready @%dHz", kSampleRate);
        }
    }
    if (!s_ready)
        ESP_LOGW(TAG, "es8311 not found, audio cases disabled (bare devkit?)");

    xTaskCreate(capture_task, "a_cap", kCaptureStack, nullptr, 10, nullptr);
    xTaskCreate(tone_task, "a_tone", kToneStack, nullptr, 7, nullptr);
    return ESP_OK;
}
