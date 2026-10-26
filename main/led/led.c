#include "config.h"

#if FEATURE_LED

#include "led/led.h"

#include <stddef.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "led";

#define MODE LEDC_LOW_SPEED_MODE
#define TIMER LEDC_TIMER_1  // the buzzer has timer 0
#define CHANNEL LEDC_CHANNEL_2
#define DUTY_BITS LEDC_TIMER_8_BIT
#define FREQ_HZ 5000

typedef struct {
    uint8_t pct;  // brightness, of LED_MAX_PCT
    uint16_t ms;  // 0 = end
} step_t;

static const step_t BLINK[] = {{100, 80}, {0, 0}};
static const step_t SIREN[] = {
    {100, 70}, {0, 70}, {100, 70}, {0, 290}, {100, 70}, {0, 70}, {100, 70}, {0, 290},
    {100, 70}, {0, 70}, {100, 70}, {0, 290}, {100, 70}, {0, 70}, {100, 70}, {0, 290},
    {100, 70}, {0, 70}, {100, 70}, {0, 290}, {100, 70}, {0, 70}, {100, 70}, {0, 290},
    {100, 70}, {0, 70}, {100, 70}, {0, 290}, {100, 70}, {0, 70}, {100, 70}, {0, 0},
};
static const step_t GLITCH[] = {{100, 30}, {0, 50}, {40, 20}, {0, 120}, {100, 15}, {0, 15},
                                {100, 15}, {0, 200}, {70, 60}, {0, 40}, {100, 25}, {0, 0}};

static bool s_ready;
static esp_timer_handle_t s_timer;
static esp_pm_lock_handle_t s_awake;
static bool s_locked;
static const step_t *s_step;  // next step, NULL when idle
static step_t s_flash[2];
static SemaphoreHandle_t s_mutex;

static void set(uint8_t pct) {
    const uint32_t duty = (uint32_t)((1u << 8) - 1) * LED_MAX_PCT * pct / 10000u;
    ledc_set_duty(MODE, CHANNEL, duty);
    ledc_update_duty(MODE, CHANNEL);
}

// Caller holds s_mutex.
static void advance(void) {
    if (!s_step || s_step->ms == 0) {
        set(0);
        s_step = NULL;
        if (s_locked) {
            esp_pm_lock_release(s_awake);
            s_locked = false;
        }
        return;
    }
    set(s_step->pct);
    const uint16_t ms = s_step->ms;
    s_step++;
    esp_timer_start_once(s_timer, (uint64_t)ms * 1000);
}

static void on_timer(void *arg) {
    (void)arg;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    advance();
    xSemaphoreGive(s_mutex);
}

static void start(const step_t *steps) {
    if (!s_ready) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_timer_stop(s_timer);
    if (steps && !s_locked && esp_pm_lock_acquire(s_awake) == ESP_OK) s_locked = true;
    s_step = steps;
    advance();
    xSemaphoreGive(s_mutex);
}

void led_init(void) {
    const ledc_timer_config_t timer = {
        .speed_mode = MODE,
        .duty_resolution = DUTY_BITS,
        .timer_num = TIMER,
        .freq_hz = FREQ_HZ,
        .clk_cfg = LEDC_USE_XTAL_CLK,
    };
    const ledc_channel_config_t channel = {
        .gpio_num = PIN_LED,
        .speed_mode = MODE,
        .channel = CHANNEL,
        .timer_sel = TIMER,
        .duty = 0,
    };
    const esp_timer_create_args_t targs = {.callback = on_timer, .name = "led"};
    s_mutex = xSemaphoreCreateMutex();
    esp_err_t err = s_mutex ? ledc_timer_config(&timer) : ESP_ERR_NO_MEM;
    if (err == ESP_OK) err = ledc_channel_config(&channel);
    if (err == ESP_OK) err = gpio_set_drive_capability(PIN_LED, (gpio_drive_cap_t)LED_DRIVE_CAP);
    if (err == ESP_OK) err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "led", &s_awake);
    if (err == ESP_OK) err = esp_timer_create(&targs, &s_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LED on GPIO%d: %s", PIN_LED, esp_err_to_name(err));
        return;
    }
    s_ready = true;
    ESP_LOGI(TAG, "LED on GPIO%d", PIN_LED);
}

void led_pattern(led_pattern_t p) {
    switch (p) {
        case LED_BLINK: start(BLINK); break;
        case LED_SIREN: start(SIREN); break;
        case LED_GLITCH: start(GLITCH); break;
        case LED_OFF: start(NULL); break;
    }
}

void led_flash(uint16_t ms) {
    if (!s_ready) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_flash[0] = (step_t){100, ms};
    s_flash[1] = (step_t){0, 0};
    xSemaphoreGive(s_mutex);
    start(s_flash);
}

#endif
