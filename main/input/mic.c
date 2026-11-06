// Analog microphone (MAX4466-type module, output biased at mid-rail) on
// PIN_MIC, sampled at MIC_SAMPLE_HZ by the ADC's DMA mode.
//
// A small task reads the DMA buffer in 10 ms blocks and runs mic_logic.c on
// them, posting what it hears as input events. It never hears our own
// buzzer: while a sound plays (input_mic_set_deaf_check) and MIC_ECHO_MS
// after, it is deaf.
//
// Power: the ADC's DMA mode keeps the chip out of light sleep, so main.c
// only listens while awake (input_mic_listen) and never at night. With no mic
// fitted the pin (pulled down) reads ~0 V, not the mic's bias: the first
// listen notices, logs it and switches the ADC off for good.

#include "config.h"

#if FEATURE_MIC

#include "driver/gpio.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input/input.h"
#include "input/mic_logic.h"

static const char *TAG = "mic";

#define BLOCK_MS (MIC_BLOCK * 1000 / MIC_SAMPLE_HZ)
#define READ_BLOCKS 4  // one DMA frame = 40 ms
#define FRAME_BYTES (MIC_BLOCK * READ_BLOCKS * SOC_ADC_DIGI_RESULT_BYTES)
#define SETTLE_MS 300   // after starting: the pin and the DC estimate settle
#define PROBE_BLOCKS 20  // the first listen checks there is a mic at all

static adc_continuous_handle_t s_adc;
static adc_channel_t s_channel;
static TaskHandle_t s_task;
static volatile bool s_want;  // main.c: listen now
static bool s_running;
static bool s_absent;
static int s_probed;  // blocks seen by the presence check
static mic_t s_mic;
static volatile int s_syllables = 1;
static bool (*volatile s_deaf_check)(void);

static uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void log_sound(void) {
    mic_sound_t s;
    if (!mic_take_sound(&s_mic, &s)) return;
    ESP_LOGI(TAG, "heard %u ms, peak %u, mean %u (floor %u), overload %u%%, %u syl -> %s", s.ms,
             s.peak, s.mean, s.floor, s.clip_pct, s.syllables,
             s.ev == INPUT_NONE ? "nothing" : input_event_name(s.ev));
}

// Only talking reaches the rest of the firmware: claps, blowing and bangs are
// not reactions any more. A shout is still talking.
static void post(input_event_t ev) {
    if (ev == INPUT_LOUD) ev = INPUT_TALK;
    if (ev != INPUT_HEAR && ev != INPUT_TALK) return;
    if (ev == INPUT_TALK) s_syllables = mic_syllables(&s_mic);
    input_post(ev);
    input_kick();
}

static void start(void) {
    if (adc_continuous_start(s_adc) != ESP_OK) return;
    s_running = true;
    s_mic.primed = false;  // the bias may have moved while we were off
    mic_deaf(&s_mic, now_ms() + SETTLE_MS);
}

static void stop(void) {
    adc_continuous_stop(s_adc);
    s_running = false;
}

static void block(const uint16_t *b, uint32_t t) {
    if (s_probed < PROBE_BLOCKS && !s_mic.deaf) {
        mic_block(&s_mic, b, MIC_BLOCK, t);
        if (++s_probed == PROBE_BLOCKS) {
            const int dc = (int)(s_mic.dc / 256);
            s_absent = dc < MIC_ABSENT_MAX;
            if (s_absent) {
                ESP_LOGW(TAG, "no mic on GPIO%d (reads %d of 4095): ears off", PIN_MIC, dc);
            } else {
                ESP_LOGI(TAG, "mic on GPIO%d, bias %d of 4095", PIN_MIC, dc);
            }
        }
        return;
    }
    bool (*check)(void) = s_deaf_check;
    if (check && check()) mic_deaf(&s_mic, t + MIC_ECHO_MS);
    post(mic_block(&s_mic, b, MIC_BLOCK, t));
    post(mic_poll(&s_mic, t));
    log_sound();
}

static void mic_task(void *arg) {
    (void)arg;
    static uint8_t raw[FRAME_BYTES];
    static uint16_t b[MIC_BLOCK];
    int nb = 0;
    for (;;) {
        if (s_absent) {
            if (s_running) stop();
            vTaskSuspend(NULL);
        }
        if (s_want != s_running) {
            if (s_want) {
                start();
                nb = 0;
            } else {
                stop();
            }
        }
        if (!s_running) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        uint32_t got = 0;
        if (adc_continuous_read(s_adc, raw, sizeof raw, &got, 100) != ESP_OK) continue;
        const uint32_t now = now_ms();
        const int n = (int)(got / SOC_ADC_DIGI_RESULT_BYTES);
        int left = n / MIC_BLOCK;  // blocks still to come in this read: dates them
        for (int i = 0; i < n; i++) {
            const adc_digi_output_data_t *r = (const adc_digi_output_data_t *)&raw[i * SOC_ADC_DIGI_RESULT_BYTES];
            if (r->type2.unit != 0 || r->type2.channel != s_channel) continue;
            b[nb++] = (uint16_t)r->type2.data;
            if (nb == MIC_BLOCK) {
                nb = 0;
                block(b, now - (uint32_t)(--left > 0 ? left : 0) * BLOCK_MS);
            }
        }
    }
}

static esp_err_t init(void) {
    adc_unit_t unit;
    esp_err_t err = adc_continuous_io_to_channel(PIN_MIC, &unit, &s_channel);
    if (err == ESP_OK && unit != ADC_UNIT_1) err = ESP_ERR_NOT_SUPPORTED;  // ADC2 has no DMA mode on the C3
    const adc_continuous_handle_cfg_t hcfg = {
        .max_store_buf_size = FRAME_BYTES * 3,
        .conv_frame_size = FRAME_BYTES,
        .flags = {.flush_pool = 1},  // fell behind: drop old samples, not new
    };
    if (err == ESP_OK) err = adc_continuous_new_handle(&hcfg, &s_adc);
    adc_digi_pattern_config_t pattern = {
        .atten = ADC_ATTEN_DB_12,  // 0-3.1 V: the mic swings around 1.65 V
        .channel = (uint8_t)s_channel,
        .unit = ADC_UNIT_1,
        .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH,
    };
    const adc_continuous_config_t cfg = {
        .pattern_num = 1,
        .adc_pattern = &pattern,
        .sample_freq_hz = MIC_SAMPLE_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    if (err == ESP_OK) err = adc_continuous_config(s_adc, &cfg);
    // With no mic the pin must read 0 V, not float around the mic's bias.
    if (err == ESP_OK) err = gpio_pulldown_en(PIN_MIC);
    mic_init(&s_mic);
    if (err == ESP_OK && xTaskCreate(mic_task, "mic", 3072, NULL, 2, &s_task) != pdPASS) err = ESP_ERR_NO_MEM;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO%d: %s", PIN_MIC, esp_err_to_name(err));
        s_absent = true;
    }
    return err;
}

void input_mic_listen(bool on) {
    if (s_want == on || !s_task || s_absent) return;
    s_want = on;
    xTaskNotifyGive(s_task);
}

void input_mic_set_deaf_check(bool (*check)(void)) {
    s_deaf_check = check;
}

int input_mic_syllables(void) {
    return s_syllables;
}

int input_mic_loudness(void) {
    if (s_absent || !s_task) return -1;
    return s_running ? mic_loudness(&s_mic) : 0;
}

const input_source_t input_source_mic = {
    .name = "mic",
    .init = init,
    .poll = NULL,
    .arm_wakeup = NULL,
    .busy = NULL,
};

#else

#include <stdbool.h>

#include "input/input.h"

void input_mic_listen(bool on) {
    (void)on;
}

void input_mic_set_deaf_check(bool (*check)(void)) {
    (void)check;
}

int input_mic_syllables(void) {
    return 1;
}

int input_mic_loudness(void) {
    return -1;
}

#endif
