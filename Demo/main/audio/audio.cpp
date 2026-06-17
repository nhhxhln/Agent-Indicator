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
#include "drivers/wm8960.h"
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

constexpr int kSampleRate = 48000;            /* MCLK=48k×256=12.288MHz,贴近参考工程干净工作点 */
constexpr int kChunkSamples = 480;            /* 10ms @48k(立体声=240 帧) */
constexpr int kRecMaxSec = 30;
constexpr int kCaptureStack = 6144;           /* 文件 IO + log */
constexpr int kPlayerStack = 8192;            /* vfs + 解析 + log */
constexpr int kToneStack = 4096;

i2s_chan_handle_t s_tx, s_rx;
enum codec_kind { CODEC_NONE, CODEC_WM8960 };
codec_kind s_codec_kind = CODEC_NONE;
bool s_ready = false;
volatile bool s_tx_swap = false;             /* 调试:TX 16bit 字节序翻转(测大小端) */
volatile int s_tx_scale = 100;               /* 输出数字缩放 %(scale 命令调;已证实非削顶,默认回满) */
int s_volume = 25;                           /* 当前音量影子值(WM8960 只写不可读,软件跟踪) */

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

/* 单声道 PCM 复制成左右两声道写出(WM8960 是立体声 I2S) */
void write_mono_stereo(const int16_t *mono, size_t samples)
{
    static int16_t st[512]; /* 256 帧;调用者已持 s_tx_mtx */
    size_t i = 0;
    while (i < samples) {
        size_t n = 0;
        for (; n < 256 && i < samples; n++, i++) {
            int16_t m = (int16_t)((int32_t)mono[i] * s_tx_scale / 100);
            int16_t v = s_tx_swap ? (int16_t)__builtin_bswap16((uint16_t)m) : m;
            st[2 * n] = v;
            st[2 * n + 1] = v;
        }
        size_t w;
        i2s_channel_write(s_tx, st, n * 2 * sizeof(int16_t), &w, portMAX_DELAY);
    }
}

void play_pcm(const int16_t *pcm, size_t samples)
{
    if (!s_ready) return;
    xSemaphoreTake(s_tx_mtx, portMAX_DELAY);
    pa_enable(true);
    write_mono_stereo(pcm, samples);
    pa_enable(false);
    xSemaphoreGive(s_tx_mtx);
}

/* ----------------------------------------------------------- 波形发生器 */
enum class Wave { kSine, kSquare, kTri, kSaw };

/* 在 s_tx 上连续生成并播放指定波形(分块,边算边送,任意时长无需大缓冲)。
 * freq: Hz;ms: 时长;amp: 峰值幅度 0..32767;duty: 方波占空比 %(仅方波用)。*/
void play_wave(Wave wf, int freq, int ms, int amp, int duty)
{
    if (!s_ready || freq <= 0) return;
    if (amp < 0) amp = 0;
    if (amp > 32767) amp = 32767;
    float dduty = (duty <= 0 || duty >= 100) ? 0.5f : duty / 100.0f;
    int64_t total = (int64_t)kSampleRate * ms / 1000; /* 总采样点 */
    static int16_t buf[1024];
    double t = 0.0;                          /* 累积周期相位 */
    double dt = (double)freq / kSampleRate;  /* 每采样推进的周期数 */

    xSemaphoreTake(s_tx_mtx, portMAX_DELAY);
    pa_enable(true);
    int64_t done = 0;
    while (done < total) {
        int n = 0;
        for (; n < 1024 && done < total; n++, done++) {
            double frac = t - (int64_t)t; /* 当前周期内位置 0..1 */
            float v;
            switch (wf) {
            case Wave::kSquare: v = frac < dduty ? 1.0f : -1.0f; break;
            case Wave::kTri:    v = frac < 0.5f ? (4.0f * frac - 1.0f)
                                                : (3.0f - 4.0f * frac); break;
            case Wave::kSaw:    v = 2.0f * (float)frac - 1.0f; break;
            case Wave::kSine:
            default:            v = sinf(2.0f * (float)M_PI * (float)frac); break;
            }
            buf[n] = (int16_t)(amp * v);
            t += dt;
        }
        write_mono_stereo(buf, n); /* 已持 s_tx_mtx */
    }
    pa_enable(false);
    xSemaphoreGive(s_tx_mtx);
}

/* ------------------------------------------------------------- capture */

void capture_task(void *)
{
    static int16_t chunk[kChunkSamples];      /* 立体声交织 L,R,L,R... */
    static int16_t mono[kChunkSamples / 2];   /* 取左声道供 VU/录音 */
    while (true) {
        if (!s_ready) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        size_t got = 0;
        if (i2s_channel_read(s_rx, chunk, sizeof(chunk), &got, pdMS_TO_TICKS(100)) != ESP_OK)
            continue;
        size_t n = got / (2 * sizeof(int16_t)); /* 立体声帧数 */
        for (size_t i = 0; i < n; i++) mono[i] = chunk[2 * i]; /* 左声道 */

        /* VU:RMS → dB,映射 0..60 给拾音条 */
        uint64_t acc = 0;
        for (size_t i = 0; i < n; i++) acc += (int32_t)mono[i] * mono[i];
        float rms = sqrtf((float)acc / (n ? n : 1));
        float db = 20.0f * log10f(rms / 32768.0f) + 90.0f; /* 噪声底 ~30 */
        g_app.mic_level_db = (uint8_t)fmaxf(0, fminf(60, db - 30));

        switch (s_cap) {
        case Capture::kRecRam: {
            size_t take = n < s_ram_cap - s_ram_len ? n : s_ram_cap - s_ram_len;
            memcpy(s_ram_buf + s_ram_len, mono, take * 2);
            s_ram_len += take;
            if (s_ram_len >= s_ram_cap) {
                s_cap = Capture::kNone;
                ESP_LOGI(TAG, "rec done: %u samples", (unsigned)s_ram_len);
            }
            break;
        }
        case Capture::kRecSd: {
            size_t take = n < s_rec_left ? n : s_rec_left;
            fwrite(mono, 2, take, s_rec_file);
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
            /* 只回灌左声道(咪头)复制成双声道,避免把右路悬空噪声送进喇叭 */
            static int16_t lb[kChunkSamples];
            for (size_t i = 0; i < n; i++) {
                int16_t v = (int16_t)((int32_t)mono[i] * s_tx_scale / 100);
                lb[2 * i] = v; lb[2 * i + 1] = v;
            }
            size_t w;
            i2s_channel_write(s_tx, lb, n * 2 * sizeof(int16_t), &w, 0); /* 欠载就丢 */
            break;
        }
        default:
            break;
        }
    }
}

/* ---------------------------------------------------------------- tone */

struct ToneNote { int freq; int ms; };
const ToneNote kTones[][4] = {
    { { 880, 90 }, { 1320, 140 }, { 0, 0 }, { 0, 0 } },     /* 0 done  叮 */
    { { 1047, 180 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },        /* 1 attention 单音 */
    { { 220, 250 }, { 196, 250 }, { 0, 0 }, { 0, 0 } },     /* 2 error 低沉双降 */
    { { 523, 80 }, { 659, 80 }, { 784, 120 }, { 0, 0 } },   /* 3 boot 上行三音 */
    { { 523, 90 }, { 659, 90 }, { 784, 90 }, { 1047, 160 } },/* 4 success 大三和弦琶音 */
    { { 988, 120 }, { 659, 200 }, { 0, 0 }, { 0, 0 } },     /* 5 notify 叮-咚 */
    { { 1568, 70 }, { 0, 70 }, { 1568, 70 }, { 0, 0 } },    /* 6 warning 双短高鸣 */
    { { 2093, 40 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },         /* 7 tick 极短滴 */
    { { 784, 90 }, { 659, 90 }, { 523, 90 }, { 392, 160 } },/* 8 shutdown 下行四音 */
    { { 1319, 60 }, { 1319, 60 }, { 1319, 90 }, { 0, 0 } }, /* 9 message 三连提示 */
};
constexpr int kToneCount = sizeof(kTones) / sizeof(kTones[0]);

void tone_task(void *)
{
    static int16_t buf[kSampleRate / 2]; /* 最长 500ms 音符 */
    uint8_t id;
    while (true) {
        if (xQueueReceive(s_tone_q, &id, portMAX_DELAY) != pdTRUE) continue;
        if (id >= kToneCount) continue;
        for (const auto &note : kTones[id]) {
            if (note.freq == 0 && note.ms == 0) break;       /* 终止符 */
            if (note.freq == 0) {                            /* 休止符:静音间隔 */
                vTaskDelay(pdMS_TO_TICKS(note.ms));
                continue;
            }
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
        write_mono_stereo(io_buf, n);
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

/* 诊断:统计 + hexdump PSRAM 录音缓冲,看 ADC 是否真的采到变化的数据 */
int cmd_dump(int argc, char **argv)
{
    if (!s_ram_buf || s_ram_len == 0) {
        printf("nothing recorded — 先跑 audio_rec [sec]\n");
        return 1;
    }
    size_t show = argc >= 2 ? (size_t)atoi(argv[1]) : 64;
    if (show > s_ram_len) show = s_ram_len;

    int16_t mn = 32767, mx = -32768;
    int64_t sum = 0;
    uint64_t sq = 0;
    size_t nz = 0;
    for (size_t i = 0; i < s_ram_len; i++) {
        int16_t v = s_ram_buf[i];
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
        sq += (int32_t)v * v;
        if (v != 0) nz++;
    }
    float rms = sqrtf((float)sq / s_ram_len);
    printf("samples=%u  min=%d max=%d span=%d  mean=%.1f rms=%.1f  nonzero=%u/%u\n",
           (unsigned)s_ram_len, mn, mx, mx - mn, (double)sum / s_ram_len, rms,
           (unsigned)nz, (unsigned)s_ram_len);
    printf("判读: span/rms 接近 0 或 nonzero≈0 → ADC/I2S 没采到信号;明显变化 → RX 链路 OK\n");
    for (size_t i = 0; i < show; i++) {
        if (i % 8 == 0) printf("\n[%04u]", (unsigned)i);
        printf(" %04x", (uint16_t)s_ram_buf[i]);
    }
    printf("\n");
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

/* 调试:运行时直写 WM8960 寄存器,A/B 测 I2S 格式/极性/增益(只写不可读) */
int cmd_wmreg(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: wmreg <reg_hex> <val_hex>\n"
               "  常用: R7(0x07)接口格式 R10/R11(0a/0b)DAC数字音量\n");
        return 1;
    }
    unsigned reg = strtoul(argv[1], nullptr, 16);
    unsigned val = strtoul(argv[2], nullptr, 16);
    esp_err_t e = wm8960_write_reg((uint8_t)reg, (uint16_t)val);
    printf("wmreg R%u(0x%02X) <= 0x%03X : %s\n", reg, reg, val & 0x1FF,
           e == ESP_OK ? "ok" : "FAIL");
    return e == ESP_OK ? 0 : 1;
}

int cmd_swap(int argc, char **argv)
{
    s_tx_swap = !(argc >= 2 && strcmp(argv[1], "off") == 0);
    printf("tx byte-swap = %s\n", s_tx_swap ? "ON" : "off");
    return 0;
}

int cmd_scale(int argc, char **argv)
{
    if (argc >= 2) {
        int p = atoi(argv[1]);
        if (p < 0) p = 0;
        if (p > 100) p = 100;
        s_tx_scale = p;
    }
    printf("tx scale = %d%%\n", s_tx_scale);
    return 0;
}

int cmd_reinit(int argc, char **argv)
{
    esp_err_t e = audio_reinit();
    printf("audio reinit: %s\n", esp_err_to_name(e));
    return e == ESP_OK ? 0 : 1;
}

int cmd_vol(int argc, char **argv)
{
    if (!s_ready) { printf("audio not ready\n"); return 1; }
    if (argc >= 2) audio_set_volume(atoi(argv[1])); /* 带参=设置 */
    printf("volume=%d%% (%s)\n", s_volume, audio_codec_name()); /* 不带参=读当前(影子值) */
    return 0;
}

int cmd_tone(int argc, char **argv)
{
    int id = argc >= 2 ? atoi(argv[1]) : 0;
    if (id < 0 || id >= kToneCount) {
        printf("tone 0..%d (done/attention/error/boot/success/notify/"
               "warning/tick/shutdown/message)\n", kToneCount - 1);
        return 1;
    }
    audio_play_tone((uint8_t)id);
    return 0;
}

int cmd_wave(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: wave <sine|square|tri|saw> <freq_hz> [ms] [amp] [duty%%]\n"
               "  例: wave sine 1000 / wave square 440 500 6000 25\n");
        return 1;
    }
    Wave wf;
    if (!strcmp(argv[1], "sine")) wf = Wave::kSine;
    else if (!strcmp(argv[1], "square")) wf = Wave::kSquare;
    else if (!strcmp(argv[1], "tri")) wf = Wave::kTri;
    else if (!strcmp(argv[1], "saw")) wf = Wave::kSaw;
    else { printf("wave: sine|square|tri|saw\n"); return 1; }
    int freq = atoi(argv[2]);
    int ms = argc >= 4 ? atoi(argv[3]) : 1000;
    int amp = argc >= 5 ? atoi(argv[4]) : 8000;
    int duty = argc >= 6 ? atoi(argv[5]) : 50;
    if (ms > 10000) ms = 10000;
    if (freq <= 0 || freq > kSampleRate / 2) {
        printf("freq 1..%d Hz\n", kSampleRate / 2);
        return 1;
    }
    printf("wave %s %dHz %dms amp=%d\n", argv[1], freq, ms, amp);
    play_wave(wf, freq, ms, amp, duty);
    return 0;
}

} // namespace

extern "C" void case_audio_register(void)
{
    const esp_console_cmd_t cmds[] = {
        { "audio_rec", "audio_rec [sec] - 录音到 PSRAM", nullptr, cmd_rec, nullptr },
        { "audio_play", "回放 PSRAM 录音", nullptr, cmd_play, nullptr },
        { "audio_dump", "audio_dump [n] - 统计+hexdump PSRAM 录音", nullptr, cmd_dump, nullptr },
        { "audio_rec_sd", "audio_rec_sd <name> [sec] - 录音到 SD", nullptr, cmd_rec_sd, nullptr },
        { "audio_player", "audio_player <path> - 播放 WAV", nullptr, cmd_player, nullptr },
        { "audio_loop", "audio_loop [on|off] - MIC->SPK 直通", nullptr, cmd_loop, nullptr },
        { "audio_vol", "audio_vol <0-100>", nullptr, cmd_vol, nullptr },
        { "tone", "tone <0-9> - 提示音(done/success/notify/warning/...)", nullptr, cmd_tone, nullptr },
        { "wave", "wave <sine|square|tri|saw> <freq> [ms] [amp] [duty] - 波形发生器", nullptr, cmd_wave, nullptr },
        { "reinit", "reinit - 重新初始化 WM8960(热复位无声时恢复)", nullptr, cmd_reinit, nullptr },
        { "wmreg", "wmreg <reg> <val> - 写 WM8960 寄存器(hex,调试)", nullptr, cmd_wmreg, nullptr },
        { "swap", "swap [on|off] - TX 16bit 字节序翻转(测大小端)", nullptr, cmd_swap, nullptr },
        { "scale", "scale [0-100] - 输出数字电平缩放%(测削顶)", nullptr, cmd_scale, nullptr },
    };
    for (auto &c : cmds) ESP_ERROR_CHECK(esp_console_cmd_register(&c));
}

extern "C" void audio_play_tone(uint8_t tone_id)
{
    if (s_tone_q) xQueueSend(s_tone_q, &tone_id, 0);
}

extern "C" bool audio_ready_c(void) { return s_ready; }

extern "C" const char *audio_codec_name(void)
{
    return s_codec_kind == CODEC_WM8960 ? "WM8960" : "none";
}

extern "C" void audio_set_volume(int vol)
{
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    s_volume = vol;
    if (s_codec_kind == CODEC_WM8960) wm8960_set_volume(vol);
}

extern "C" int audio_get_volume(void) { return s_volume; }

/* 重新初始化 codec(重跑 WM8960 复位+PLL+寄存器),热复位偶发无声时手动恢复。
 * I2S 通道不动(主时钟一直在跑),只重配 codec。 */
extern "C" esp_err_t audio_reinit(void)
{
    if (!wm8960_probe()) return ESP_ERR_NOT_FOUND;
    esp_err_t e = wm8960_init(kSampleRate);
    if (e == ESP_OK) {
        s_codec_kind = CODEC_WM8960;
        s_ready = true;
        audio_set_volume(s_volume);
    }
    return e;
}

extern "C" esp_err_t audio_start(void)
{
    s_tx_mtx = xSemaphoreCreateMutex();
    s_tone_q = xQueueCreate(4, sizeof(uint8_t));

    /* WM8960 当 I2S 主(用板载 24MHz 晶振产生 BCLK/WS),ESP32 当从 → 单一时钟域,
     * 消除"ESP32 内部时钟 vs codec 晶振"不相干导致的间歇无声。 */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    chan_cfg.auto_clear = true; /* TX 欠载自动补零,否则会重播上一块 DMA 缓冲(听起来循环播放) */
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx, &s_rx), TAG, "chan");

    i2s_std_config_t std_cfg = {
        /* 显式写全 clk_cfg 字段(含 ext_clk_freq_hz),消除 -Wmissing-field-initializers */
        .clk_cfg = {
            .sample_rate_hz = kSampleRate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            /* ES8311 用 SCLK 当 MCLK 可不接;WM8960 需要 MCLK(BOARD_I2S_MCLK) */
            .mclk = (BOARD_I2S_MCLK >= 0) ? (gpio_num_t)BOARD_I2S_MCLK : I2S_GPIO_UNUSED,
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

    /* 探测 WM8960(0x1A,Demo 默认 codec) */
    if (wm8960_probe() && wm8960_init(kSampleRate) == ESP_OK) {
        s_codec_kind = CODEC_WM8960;
        s_ready = true;
        audio_set_volume(s_volume); /* 上电默认音量(25%) */
    } else {
        ESP_LOGW(TAG, "wm8960 not found, audio disabled");
    }

    xTaskCreate(capture_task, "a_cap", kCaptureStack, nullptr, 10, nullptr);
    xTaskCreate(tone_task, "a_tone", kToneStack, nullptr, 7, nullptr);
    return ESP_OK;
}
