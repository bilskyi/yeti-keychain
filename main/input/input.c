#include "input/input.h"

#include "config.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "input";

#define QUEUE_LEN 16
#define MAX_SUBSCRIBERS 4

static const input_source_t *const SOURCES[] = {
#if FEATURE_BUTTONS
    &input_source_buttons,
#endif
#if FEATURE_ACCEL
    &input_source_accel,
#endif
    NULL,  // keeps the array non-empty when every flag is off
};
#define SOURCE_COUNT ((int)(sizeof(SOURCES) / sizeof(SOURCES[0])) - 1)

static QueueHandle_t s_queue;
static SemaphoreHandle_t s_kick;
static bool s_gpio_isr_ready;
static uint32_t s_last_button;  // time of the last button event
static bool s_button_seen;
static struct {
    input_handler_t fn;
    void *ctx;
} s_subs[MAX_SUBSCRIBERS];
static int s_nsubs;

void input_init(void) {
    s_queue = xQueueCreate(QUEUE_LEN, sizeof(input_event_t));
    s_kick = xSemaphoreCreateBinary();
    if (!s_queue || !s_kick) {
        ESP_LOGE(TAG, "no memory for the event queue");
        return;
    }
    for (int i = 0; i < SOURCE_COUNT; i++) {
        const esp_err_t err = SOURCES[i]->init ? SOURCES[i]->init() : ESP_OK;
        ESP_LOGI(TAG, "source %s: %s", SOURCES[i]->name, esp_err_to_name(err));
    }
    if (SOURCE_COUNT == 0) ESP_LOGI(TAG, "no input sources enabled");
}

bool input_subscribe(input_handler_t fn, void *ctx) {
    if (s_nsubs >= MAX_SUBSCRIBERS) return false;
    s_subs[s_nsubs].fn = fn;
    s_subs[s_nsubs].ctx = ctx;
    s_nsubs++;
    return true;
}

bool input_post(input_event_t ev) {
    return s_queue && xQueueSend(s_queue, &ev, 0) == pdTRUE;
}

bool IRAM_ATTR input_post_from_isr(input_event_t ev) {
    BaseType_t woken = pdFALSE;
    const bool ok = s_queue && xQueueSendFromISR(s_queue, &ev, &woken) == pdTRUE;
    if (woken) portYIELD_FROM_ISR();
    input_kick_from_isr();
    return ok;
}

void IRAM_ATTR input_kick_from_isr(void) {
    if (!s_kick) return;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_kick, &woken);
    if (woken) portYIELD_FROM_ISR();
}

void input_wait(uint32_t timeout_ms) {
    const TickType_t ticks = pdMS_TO_TICKS(timeout_ms) ? pdMS_TO_TICKS(timeout_ms) : 1;
    if (s_kick) {
        xSemaphoreTake(s_kick, ticks);
    } else {
        vTaskDelay(ticks);
    }
}

esp_err_t input_gpio_isr_init(void) {
    if (s_gpio_isr_ready) return ESP_OK;
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;  // INVALID_STATE: already in
    err = esp_sleep_enable_gpio_wakeup();
    if (err == ESP_OK) s_gpio_isr_ready = true;
    return err;
}

void input_poll(uint32_t now_ms) {
    for (int i = 0; i < SOURCE_COUNT; i++) {
        if (SOURCES[i]->poll) SOURCES[i]->poll(now_ms);
    }
}

static bool is_button(input_event_t ev) {
    return ev >= INPUT_BTN_A_SHORT && ev <= INPUT_BTN_C_PRESS;
}

// A tap that is really the jolt of a button press: a button is held now, or
// one produced an event moments ago. The tap's own event comes ~450 ms after
// the click (it waits for a possible second tap), so it usually arrives after
// the button's SHORT, or while the button is still held on the way to a LONG.
static bool tap_from_button(input_event_t ev, uint32_t now_ms) {
    if (ev != INPUT_TAP && ev != INPUT_DOUBLE_TAP) return false;
#if FEATURE_BUTTONS
    if (input_source_buttons.busy(now_ms)) return true;
#endif
    return s_button_seen && (int32_t)(now_ms - s_last_button) < TAP_AFTER_BUTTON_MS;
}

void input_dispatch(uint32_t now_ms) {
    if (!s_queue) return;
    input_event_t ev;
    while (xQueueReceive(s_queue, &ev, 0) == pdTRUE) {
        if (is_button(ev)) {
            s_last_button = now_ms;
            s_button_seen = true;
        }
        if (tap_from_button(ev, now_ms)) {
            ESP_LOGI(TAG, "event %s ignored (button press)", input_event_name(ev));
            continue;
        }
        // PRESS precedes every SHORT/LONG: logging it too would double the noise.
        if (ev < INPUT_BTN_A_PRESS) ESP_LOGI(TAG, "event %s", input_event_name(ev));
        for (int i = 0; i < s_nsubs; i++) s_subs[i].fn(ev, now_ms, s_subs[i].ctx);
    }
}

bool input_busy(uint32_t now_ms) {
    for (int i = 0; i < SOURCE_COUNT; i++) {
        if (SOURCES[i]->busy && SOURCES[i]->busy(now_ms)) return true;
    }
    return false;
}

int input_source_count(void) {
    return SOURCE_COUNT;
}

void input_arm_wakeup(void) {
    for (int i = 0; i < SOURCE_COUNT; i++) {
        if (SOURCES[i]->arm_wakeup) SOURCES[i]->arm_wakeup();
    }
}
