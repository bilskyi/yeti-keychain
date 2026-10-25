#include "config.h"

#if FEATURE_SOUND

#include "sound/sound.h"

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sound";

#define MODE LEDC_LOW_SPEED_MODE
#define TIMER LEDC_TIMER_0
#define CHANNEL LEDC_CHANNEL_0
#define CHANNEL_BRIDGE LEDC_CHANNEL_1
#define DUTY_BITS LEDC_TIMER_10_BIT
#define DUTY_ON ((1u << 10) * SOUND_DUTY_PCT / 100)

typedef struct {
    uint16_t hz;  // 0 = rest
    uint16_t ms;  // 0 = end of the tune
} note_t;

// Short on purpose: every one is under half a second. Pitches sit at
// 1.5-4 kHz, around a small buzzer's resonance (~2-3 kHz): below ~1 kHz the
// same buzzer is many times quieter, which mattered with no transistor.
static const note_t HELLO[] = {{2093, 60}, {2637, 60}, {3136, 90}, {0, 0}};
static const note_t TAP[] = {{2637, 40}, {3520, 60}, {0, 0}};
static const note_t DOUBLE_TAP[] = {{2637, 70}, {0, 40}, {2637, 70}, {3520, 150}, {0, 0}};
static const note_t SHAKE[] = {{2000, 35}, {1600, 35}, {2000, 35}, {1600, 35},
                               {2000, 35}, {1600, 35}, {1400, 90}, {0, 0}};
static const note_t TILT[] = {{2349, 25}, {0, 0}};
static const note_t CLICK_A[] = {{3520, 25}, {0, 0}};
static const note_t CLICK_BC[] = {{2794, 20}, {0, 0}};
static const note_t HAPPY[] = {{2637, 50}, {3136, 50}, {4186, 80}, {0, 0}};
static const note_t GOODNIGHT[] = {{3136, 90}, {2349, 90}, {1568, 180}, {0, 0}};
static const note_t UNMUTED[] = {{2093, 50}, {3136, 80}, {0, 0}};
static const note_t MUTED[] = {{1568, 60}, {1047, 80}, {0, 0}};

static RTC_DATA_ATTR bool s_muted;  // kept through deep sleep
static bool s_ready;
static esp_timer_handle_t s_timer;
static esp_pm_lock_handle_t s_awake;
static const note_t *s_tune;  // current note, NULL when silent
static bool s_locked;
// play() runs in the main task, next_note() in the esp_timer task.
static SemaphoreHandle_t s_mutex;

// Bridge: the second pin carries the same pulse shifted by half a period, so
// the two are high in turn and the buzzer sees +3.3 V then -3.3 V. At rest
// both duties are 0 and both pins sit low, so no DC flows through the coil.
static void tone(uint16_t hz) {
    if (hz) ledc_set_freq(MODE, TIMER, hz);
    const uint32_t duty = hz ? DUTY_ON : 0;
    ledc_set_duty_with_hpoint(MODE, CHANNEL, duty, 0);
    ledc_update_duty(MODE, CHANNEL);
#if PIN_BUZZER_BRIDGE >= 0
    ledc_set_duty_with_hpoint(MODE, CHANNEL_BRIDGE, duty, 1u << (DUTY_BITS - 1));
    ledc_update_duty(MODE, CHANNEL_BRIDGE);
#endif
}

static void finish(void) {
    tone(0);
    s_tune = NULL;
    if (s_locked) {
        esp_pm_lock_release(s_awake);
        s_locked = false;
    }
}

// Caller holds s_mutex.
static void step(void) {
    if (!s_tune || s_tune->ms == 0) {
        finish();
        return;
    }
    tone(s_tune->hz);
    const uint16_t ms = s_tune->ms;
    s_tune++;
    esp_timer_start_once(s_timer, (uint64_t)ms * 1000);
}

static void next_note(void *arg) {
    (void)arg;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    step();
    xSemaphoreGive(s_mutex);
}

static void play(const note_t *tune, bool even_if_muted) {
    if (!s_ready || (s_muted && !even_if_muted)) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_timer_stop(s_timer);  // a new sound cuts the old one off
    if (!s_locked && esp_pm_lock_acquire(s_awake) == ESP_OK) s_locked = true;
    s_tune = tune;
    step();
    xSemaphoreGive(s_mutex);
}

void sound_init(void) {
    const ledc_timer_config_t timer = {
        .speed_mode = MODE,
        .duty_resolution = DUTY_BITS,
        .timer_num = TIMER,
        .freq_hz = 1000,
        .clk_cfg = LEDC_USE_XTAL_CLK,  // fixed 40 MHz, unaffected by DFS
    };
    const ledc_channel_config_t channel = {
        .gpio_num = PIN_BUZZER,
        .speed_mode = MODE,
        .channel = CHANNEL,
        .timer_sel = TIMER,
        .duty = 0,
        .flags = {.output_invert = SOUND_ACTIVE_LOW},
    };
#if PIN_BUZZER_BRIDGE >= 0
    ledc_channel_config_t bridge = channel;
    bridge.gpio_num = PIN_BUZZER_BRIDGE;
    bridge.channel = CHANNEL_BRIDGE;
#endif
    const esp_timer_create_args_t targs = {.callback = next_note, .name = "sound"};
    s_mutex = xSemaphoreCreateMutex();
    esp_err_t err = s_mutex ? ledc_timer_config(&timer) : ESP_ERR_NO_MEM;
    if (err == ESP_OK) err = ledc_channel_config(&channel);
    if (err == ESP_OK) err = gpio_set_drive_capability(PIN_BUZZER, (gpio_drive_cap_t)SOUND_DRIVE_CAP);
#if PIN_BUZZER_BRIDGE >= 0
    if (err == ESP_OK) err = ledc_channel_config(&bridge);
    if (err == ESP_OK) {
        err = gpio_set_drive_capability(PIN_BUZZER_BRIDGE, (gpio_drive_cap_t)SOUND_DRIVE_CAP);
    }
#endif
    if (err == ESP_OK) err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "sound", &s_awake);
    if (err == ESP_OK) err = esp_timer_create(&targs, &s_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "buzzer on GPIO%d: %s", PIN_BUZZER, esp_err_to_name(err));
        return;
    }
    s_ready = true;
#if PIN_BUZZER_BRIDGE >= 0
    ESP_LOGI(TAG, "buzzer on GPIO%d / GPIO%d (bridge)%s", PIN_BUZZER, PIN_BUZZER_BRIDGE,
             s_muted ? " (muted)" : "");
#else
    ESP_LOGI(TAG, "buzzer on GPIO%d%s", PIN_BUZZER, s_muted ? " (muted)" : "");
#endif
}

void sound_hello(void) {
    play(HELLO, false);
}

void sound_on_event(input_event_t ev) {
    const note_t *tune = NULL;
    switch (ev) {
        case INPUT_TAP: tune = TAP; break;
        case INPUT_DOUBLE_TAP: tune = DOUBLE_TAP; break;
        case INPUT_SHAKE: tune = SHAKE; break;
        case INPUT_TILT_LEFT:
        case INPUT_TILT_RIGHT: tune = TILT; break;
        case INPUT_BTN_A_SHORT: tune = CLICK_A; break;
        case INPUT_BTN_B_SHORT:
        case INPUT_BTN_C_SHORT: tune = CLICK_BC; break;
        case INPUT_BTN_C_LONG: tune = HAPPY; break;
        case INPUT_BTN_A_LONG: tune = GOODNIGHT; break;
        case INPUT_BTN_B_LONG:
            s_muted = !s_muted;
            ESP_LOGI(TAG, "%s", s_muted ? "muted" : "unmuted");
            play(s_muted ? MUTED : UNMUTED, true);  // say so either way
            return;
        case INPUT_NONE:
        case INPUT_EVENT_COUNT: break;
    }
    if (tune) play(tune, false);
}

#endif
