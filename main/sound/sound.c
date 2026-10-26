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
// Menu and game effects, indexed by app_sound_t.
static const note_t FX_MOVE[] = {{2794, 15}, {0, 0}};
static const note_t FX_SELECT[] = {{2349, 40}, {3136, 60}, {0, 0}};
static const note_t FX_BACK[] = {{3136, 40}, {2349, 60}, {0, 0}};
static const note_t FX_ROLL[] = {{1800, 20}, {0, 25}, {2300, 20}, {0, 25}, {1600, 20}, {0, 25},
                                 {2500, 20}, {0, 25}, {2000, 20}, {0, 0}};
static const note_t FX_DING[] = {{3520, 40}, {4186, 120}, {0, 0}};
static const note_t FX_COIN[] = {{3520, 30}, {0, 30}, {3520, 60}, {0, 0}};
static const note_t FX_GO[] = {{3136, 160}, {0, 0}};
static const note_t FX_FAIL[] = {{1047, 110}, {784, 170}, {0, 0}};
static const note_t FX_POINT[] = {{3136, 30}, {4186, 40}, {0, 0}};
static const note_t FX_FLAP[] = {{2093, 15}, {0, 0}};
static const note_t FX_OVER[] = {{2349, 100}, {1760, 100}, {1319, 200}, {0, 0}};
static const note_t FX_RECORD[] = {{2093, 60}, {2637, 60}, {3136, 60}, {4186, 180}, {0, 0}};
static const note_t FX_EAT[] = {{1760, 25}, {0, 20}, {1568, 35}, {0, 0}};
static const note_t FX_LEVELUP[] = {{2093, 70}, {2637, 70}, {3136, 70}, {0, 30}, {2637, 60},
                                    {3136, 60}, {4186, 220}, {0, 0}};
static const note_t FX_TONE_L[] = {{2093, 180}, {0, 0}};
static const note_t FX_TONE_R[] = {{3136, 180}, {0, 0}};
static const note_t FX_HI[] = {{2637, 50}, {0, 30}, {3520, 90}, {0, 0}};
static const note_t FX_SCARED[] = {{3520, 40}, {3136, 40}, {3520, 40}, {3136, 40}, {3520, 40},
                                   {3136, 40}, {0, 0}};
static const note_t FX_WHEE[] = {{1568, 40}, {1976, 40}, {2349, 40}, {2794, 40}, {3322, 40},
                                 {3951, 120}, {0, 0}};
// "ah... ah... CHOO": two rising breaths, then a burst at the "choo" frame.
static const note_t FX_SNEEZE[] = {{1760, 120}, {0, 160}, {2093, 140}, {0, 280}, {3951, 30},
                                   {2349, 30}, {1568, 60}, {0, 0}};
static const note_t FX_HUM[] = {{2349, 140}, {2637, 140}, {3136, 200}, {2637, 140}, {2349, 280},
                                {0, 0}};

static const note_t *const FX[SND_COUNT] = {
    [SND_MOVE] = FX_MOVE,     [SND_SELECT] = FX_SELECT, [SND_BACK] = FX_BACK,
    [SND_ROLL] = FX_ROLL,     [SND_DING] = FX_DING,     [SND_COIN] = FX_COIN,
    [SND_GO] = FX_GO,         [SND_FAIL] = FX_FAIL,     [SND_POINT] = FX_POINT,
    [SND_FLAP] = FX_FLAP,     [SND_OVER] = FX_OVER,     [SND_RECORD] = FX_RECORD,
    [SND_EAT] = FX_EAT,       [SND_LEVELUP] = FX_LEVELUP, [SND_TONE_L] = FX_TONE_L,
    [SND_TONE_R] = FX_TONE_R, [SND_HI] = FX_HI,         [SND_SCARED] = FX_SCARED,
    [SND_WHEE] = FX_WHEE,     [SND_SNEEZE] = FX_SNEEZE, [SND_HUM] = FX_HUM,
};

static bool s_muted;
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

static void play(const note_t *tune) {
    if (!s_ready || s_muted || !tune) return;
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
    play(HELLO);
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
        case INPUT_BTN_B_LONG:  // opens the menu, which makes its own sound
        case INPUT_BTN_A_PRESS:
        case INPUT_BTN_B_PRESS:
        case INPUT_BTN_C_PRESS:
        case INPUT_NONE:
        case INPUT_EVENT_COUNT: break;
    }
    play(tune);
}

void sound_fx(app_sound_t fx) {
    if ((unsigned)fx < SND_COUNT) play(FX[fx]);
}

void sound_set_muted(bool muted) {
    s_muted = muted;
    if (muted && s_ready) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        esp_timer_stop(s_timer);
        finish();
        xSemaphoreGive(s_mutex);
    }
}

#endif
