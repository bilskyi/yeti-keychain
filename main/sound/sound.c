#include "config.h"

#if FEATURE_SOUND

#include "sound/sound.h"
#include "sound/rtttl.h"

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "sound";

#define MODE LEDC_LOW_SPEED_MODE
#define TIMER LEDC_TIMER_0
#define CHANNEL LEDC_CHANNEL_0
#define CHANNEL_BRIDGE LEDC_CHANNEL_1
#define DUTY_BITS LEDC_TIMER_10_BIT

#define SONG_MAX 320  // notes and gaps of the longest song
#define SONG_GAP_MS 18

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
static const note_t BOOP[] = {{3136, 30}, {0, 0}};  // a pat on the head
static const note_t GIGGLE[] = {{3520, 35}, {0, 25}, {3951, 35}, {0, 25}, {3520, 35}, {0, 25},
                                {4186, 60}, {0, 0}};
// Menu and game effects, indexed by app_sound_t.
static const note_t FX_MOVE[] = {{2794, 15}, {0, 0}};
static const note_t FX_SELECT[] = {{2349, 40}, {3136, 60}, {0, 0}};
static const note_t FX_BACK[] = {{3136, 40}, {2349, 60}, {0, 0}};
static const note_t FX_ROLL[] = {{1800, 20}, {0, 25}, {2300, 20}, {0, 25}, {1600, 20}, {0, 25},
                                 {2500, 20}, {0, 25}, {2000, 20}, {0, 0}};
static const note_t FX_DING[] = {{3520, 40}, {4186, 120}, {0, 0}};
static const note_t FX_GO[] = {{3136, 160}, {0, 0}};
static const note_t FX_FAIL[] = {{1047, 110}, {784, 170}, {0, 0}};
static const note_t FX_POINT[] = {{3136, 30}, {4186, 40}, {0, 0}};
static const note_t FX_FLAP[] = {{2093, 15}, {0, 0}};
static const note_t FX_OVER[] = {{2349, 100}, {1760, 100}, {1319, 200}, {0, 0}};
static const note_t FX_RECORD[] = {{2093, 60}, {2637, 60}, {3136, 60}, {4186, 180}, {0, 0}};
static const note_t FX_HI[] = {{2637, 50}, {0, 30}, {3520, 90}, {0, 0}};
static const note_t FX_SCARED[] = {{3520, 40}, {3136, 40}, {3520, 40}, {3136, 40}, {3520, 40},
                                   {3136, 40}, {0, 0}};
static const note_t FX_WHEE[] = {{1568, 40}, {1976, 40}, {2349, 40}, {2794, 40}, {3322, 40},
                                 {3951, 120}, {0, 0}};
// "ah... ah... CHOO": two rising breaths, then a burst at the "choo" frame.
static const note_t FX_SNEEZE[] = {{1760, 120}, {0, 160}, {2093, 140}, {0, 280}, {3951, 30},
                                   {2349, 30}, {1568, 60}, {0, 0}};
// Garbled data: a burst of unrelated chirps, like a modem having a bad day.
static const note_t FX_GLITCH[] = {{3951, 20}, {1568, 30}, {0, 20}, {3322, 15}, {2093, 40},
                                   {0, 60},    {3729, 15}, {1760, 15}, {3951, 15}, {0, 40},
                                   {1568, 90}, {0, 30},    {2794, 20}, {2349, 20}, {0, 0}};
static const note_t FX_HMPH[] = {{2349, 80}, {0, 40}, {1760, 220}, {0, 0}};
static const note_t FX_HUM[] = {{2349, 140}, {2637, 140}, {3136, 200}, {2637, 140}, {2349, 280},
                                {0, 0}};

// A purr: a low warble, quiet-ish clicks. Repeated by main.c while petted.
static const note_t FX_PURR[] = {{1397, 30}, {0, 25}, {1319, 30}, {0, 25}, {1397, 30}, {0, 25},
                                 {1319, 30}, {0, 0}};
static const note_t FX_GROWL[] = {{1319, 60}, {1175, 60}, {1319, 60}, {1175, 60}, {1047, 140}, {0, 0}};
// Fury: a raging buzz swinging between low and high.
static const note_t FX_FURY[] = {{1568, 30}, {3951, 30}, {1568, 30}, {3951, 30}, {1397, 30},
                                 {3729, 30}, {1397, 30}, {3729, 30}, {1175, 200}, {0, 0}};
static const note_t FX_KO[] = {{3951, 30}, {0, 40},    {2637, 80}, {2349, 80},
                               {2093, 80}, {1760, 80}, {1397, 80}, {1175, 220}, {0, 0}};
static const note_t FX_AWW[] = {{2349, 90}, {2794, 90}, {3520, 120}, {4186, 200}, {0, 0}};
static const note_t FX_SIGH[] = {{3136, 120}, {2637, 140}, {2093, 260}, {0, 0}};
static const note_t FX_ZAP[] = {{3951, 15}, {1245, 25}, {3729, 15}, {1047, 40}, {0, 0}};
static const note_t FX_ERROR[] = {{1047, 140}, {0, 40}, {1047, 200}, {0, 0}};
static const note_t FX_POWERDOWN[] = {{3951, 50}, {3136, 50}, {2349, 60}, {1760, 70},
                                      {1319, 90}, {988, 200},  {0, 0}};
static const note_t FX_HEARTBEAT[] = {{1568, 45}, {0, 80}, {1760, 60}, {0, 0}};
static const note_t FX_SPARKLE[] = {{3136, 40}, {3520, 40}, {3951, 40}, {4186, 40}, {0, 30},
                                    {4186, 60}, {3951, 40}, {4186, 160}, {0, 0}};
static const note_t FX_BUZZ[] = {{1397, 25}, {1319, 25}, {1397, 25}, {1319, 25}, {1397, 25},
                                 {1319, 25}, {1397, 25}, {1319, 25}, {0, 0}};
static const note_t FX_TWINKLE[] = {{3951, 40}, {0, 120}, {3520, 40}, {0, 160}, {4186, 60}, {0, 0}};
static const note_t FX_ROFL[] = {{3951, 35}, {0, 25}, {3520, 35}, {0, 25}, {3951, 35}, {0, 25},
                                 {3136, 35}, {0, 25}, {3520, 35}, {0, 25}, {2794, 35}, {0, 25},
                                 {3136, 35}, {0, 25}, {2637, 35}, {0, 25}, {2794, 35}, {0, 25},
                                 {2349, 90}, {0, 0}};
// Voice game: a hop up (only between games: the mic is deaf while it plays).
static const note_t FX_JUMP[] = {{2093, 25}, {3136, 40}, {0, 0}};
static const note_t FX_STRETCH[] = {{1760, 180}, {2093, 180}, {2637, 260}, {0, 60}, {2349, 200}, {0, 0}};
// Charger in: a rising zip into a bright double chime. Out: a short drop.
static const note_t FX_CHARGE[] = {{1568, 30}, {2093, 30}, {2637, 30}, {3136, 30}, {0, 40},
                                   {3951, 60}, {0, 30}, {4186, 140}, {0, 0}};
static const note_t FX_UNPLUG[] = {{3136, 50}, {2093, 120}, {0, 0}};
// The 67 easter egg: a whine charging up, a low SIX, a higher SEVEN, a tick
// per swing of the hands, and a crumbling blast.
static const note_t FX_EGG_RISE[] = {{1047, 50}, {1319, 50}, {1568, 50}, {2093, 50}, {2637, 50},
                                     {3136, 60}, {3520, 70}, {3951, 90}, {0, 0}};
static const note_t FX_EGG_SIX[] = {{2093, 25}, {1047, 200}, {0, 0}};
static const note_t FX_EGG_SEVEN[] = {{3136, 25}, {1568, 240}, {0, 0}};
static const note_t FX_EGG_TICK[] = {{2637, 30}, {0, 0}};
static const note_t FX_EGG_BOOM[] = {{3951, 20}, {1245, 25}, {3322, 20}, {988, 30}, {2349, 20},
                                     {784, 40}, {1568, 30}, {659, 60}, {523, 160}, {0, 0}};
static const note_t FX_BOOT[] = {{1568, 60}, {0, 30}, {2093, 60}, {2637, 60}, {3136, 60}, {4186, 200}, {0, 0}};

static const note_t *const FX[SND_COUNT] = {
    [SND_MOVE] = FX_MOVE,     [SND_SELECT] = FX_SELECT, [SND_BACK] = FX_BACK,
    [SND_ROLL] = FX_ROLL,     [SND_DING] = FX_DING,     [SND_GO] = FX_GO,
    [SND_FAIL] = FX_FAIL,     [SND_POINT] = FX_POINT,   [SND_FLAP] = FX_FLAP,
    [SND_OVER] = FX_OVER,     [SND_RECORD] = FX_RECORD, [SND_HI] = FX_HI,
    [SND_SCARED] = FX_SCARED, [SND_WHEE] = FX_WHEE,     [SND_SNEEZE] = FX_SNEEZE,
    [SND_HUM] = FX_HUM,       [SND_GLITCH] = FX_GLITCH, [SND_HMPH] = FX_HMPH,
    [SND_PURR] = FX_PURR,     [SND_GROWL] = FX_GROWL,   [SND_FURY] = FX_FURY,
    [SND_KO] = FX_KO,         [SND_AWW] = FX_AWW,       [SND_SIGH] = FX_SIGH,
    [SND_ZAP] = FX_ZAP,       [SND_ERROR] = FX_ERROR,   [SND_POWERDOWN] = FX_POWERDOWN,
    [SND_BOOT] = FX_BOOT,     [SND_HEARTBEAT] = FX_HEARTBEAT, [SND_SPARKLE] = FX_SPARKLE,
    [SND_BUZZ] = FX_BUZZ,     [SND_TWINKLE] = FX_TWINKLE, [SND_ROFL] = FX_ROFL,
    [SND_STRETCH] = FX_STRETCH, [SND_JUMP] = FX_JUMP,
    [SND_CHARGE] = FX_CHARGE, [SND_UNPLUG] = FX_UNPLUG,
    [SND_EGG_RISE] = FX_EGG_RISE, [SND_EGG_SIX] = FX_EGG_SIX, [SND_EGG_SEVEN] = FX_EGG_SEVEN,
    [SND_EGG_TICK] = FX_EGG_TICK, [SND_EGG_BOOM] = FX_EGG_BOOM,
};

// Volume is the PWM duty: 50% is the loudest a passive buzzer gets, and it
// falls off fast below ~15%. Indexed by the menu's level, 0 = off.
static const uint8_t DUTY_PCT[APP_VOLUME_LEVELS] = {0, 3, 12, SOUND_DUTY_PCT};
static bool s_muted;
static uint32_t s_duty = (1u << 10) * SOUND_DUTY_PCT / 100;
static bool s_ready;
static esp_timer_handle_t s_timer;
static esp_pm_lock_handle_t s_awake;
static const note_t *volatile s_tune;  // current note, NULL when silent (read by the mic task)
static bool s_locked;
static note_t s_song[SONG_MAX];
#define BABBLE_MAX 9
static note_t s_babble[2 * BABBLE_MAX + 1];
static bool s_is_song;  // s_tune points into s_song
static bool s_hooked;   // a song or a babble: each note goes to s_note_hook
static void (*s_note_hook)(uint16_t hz, uint16_t ms);
// play() runs in the main task, next_note() in the esp_timer task.
static SemaphoreHandle_t s_mutex;

// Bridge: the second pin carries the same pulse shifted by half a period, so
// the two are high in turn and the buzzer sees +3.3 V then -3.3 V. At rest
// both duties are 0 and both pins sit low, so no DC flows through the coil.
static void tone(uint16_t hz) {
    if (hz) ledc_set_freq(MODE, TIMER, hz);
    const uint32_t duty = hz ? s_duty : 0;
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
    s_is_song = false;
    s_hooked = false;
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
    if (s_hooked && s_note_hook) s_note_hook(s_tune->hz, s_tune->ms);
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
    s_is_song = tune == s_song;
    s_hooked = s_is_song || tune == s_babble;
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

void sound_goodnight(void) {
    play(GOODNIGHT);
}

void sound_on_event(input_event_t ev) {
    const note_t *tune = NULL;
    switch (ev) {
        case INPUT_TAP: tune = TAP; break;
        case INPUT_DOUBLE_TAP: tune = DOUBLE_TAP; break;
        case INPUT_SHAKE: tune = SHAKE; break;
        case INPUT_TILT_LEFT:
        case INPUT_TILT_RIGHT: tune = TILT; break;
        case INPUT_BTN_A_SHORT:
        case INPUT_BTN_A_LONG: tune = CLICK_A; break;
        case INPUT_BTN_B_LONG: tune = CLICK_BC; break;
        case INPUT_BTN_C_SHORT:  // the status card makes its own sound
        case INPUT_BTN_C_LONG:
        case INPUT_TOUCH: tune = BOOP; break;
        case INPUT_TOUCH_HOLD: tune = FX_PURR; break;
        case INPUT_TOUCH_END: tune = HAPPY; break;
        case INPUT_TICKLE: tune = GIGGLE; break;
        case INPUT_CLAP:  // the mic only hears talking now (mic.c)
        case INPUT_DOUBLE_CLAP:
        case INPUT_LOUD:
        case INPUT_BLOW:
        case INPUT_HEAR:
        case INPUT_TALK:  // sound_babble (main.c)
        case INPUT_BTN_B_SHORT:  // opens the menu, which makes its own sound
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

bool sound_song(int i) {
    if (!s_ready || s_muted || i < 0 || i >= songs_count()) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_timer_stop(s_timer);  // s_song may be playing: stop before rewriting it
    finish();
    const int n = rtttl_parse(song_rtttl(i), s_song, SONG_MAX, SONG_GAP_MS);
    xSemaphoreGive(s_mutex);
    if (n == 0) return false;
    ESP_LOGI(TAG, "song: %s (%d notes)", song_name(i), n);
    play(s_song);
    return true;
}

void sound_stop(void) {
    if (!s_ready) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_timer_stop(s_timer);
    finish();
    xSemaphoreGive(s_mutex);
}

void sound_babble(int syllables) {
    if (!s_ready || s_muted) return;
    if (syllables < 1) syllables = 1;
    if (syllables > BABBLE_MAX) syllables = BABBLE_MAX;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_timer_stop(s_timer);  // s_babble may be playing: stop before rewriting it
    finish();
    // One chirp per syllable heard, at random pitches around the buzzer's
    // sweet spot, the last one rising like a question or falling like an answer.
    int k = 0;
    for (int i = 0; i < syllables; i++) {
        const uint32_t r = esp_random();
        s_babble[k++] = (note_t){(uint16_t)(1900 + r % 1700), (uint16_t)(50 + (r >> 12) % 70)};
        s_babble[k++] = (note_t){0, (uint16_t)(30 + (r >> 20) % 40)};
    }
    s_babble[k - 2].hz = esp_random() % 2 ? 3951 : 1760;
    s_babble[k - 2].ms = 140;
    s_babble[k - 1] = (note_t){0, 0};
    xSemaphoreGive(s_mutex);
    play(s_babble);
}

bool sound_busy(void) {
    return s_tune != NULL;
}

bool sound_song_playing(void) {
    return s_is_song;
}

void sound_set_note_hook(void (*hook)(uint16_t hz, uint16_t ms)) {
    s_note_hook = hook;
}

void sound_set_volume(int level) {
    if (level < 0) level = 0;
    if (level >= APP_VOLUME_LEVELS) level = APP_VOLUME_LEVELS - 1;
    s_muted = level == 0;
    s_duty = (1u << 10) * DUTY_PCT[level] / 100;
    if (s_muted && s_ready) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        esp_timer_stop(s_timer);
        finish();
        xSemaphoreGive(s_mutex);
    }
}

#endif
