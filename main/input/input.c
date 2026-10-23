#include "input/input.h"

#include "config.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

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
static struct {
    input_handler_t fn;
    void *ctx;
} s_subs[MAX_SUBSCRIBERS];
static int s_nsubs;

void input_init(void) {
    s_queue = xQueueCreate(QUEUE_LEN, sizeof(input_event_t));
    if (!s_queue) {
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
    return ok;
}

void input_poll(uint32_t now_ms) {
    for (int i = 0; i < SOURCE_COUNT; i++) {
        if (SOURCES[i]->poll) SOURCES[i]->poll(now_ms);
    }
}

void input_dispatch(uint32_t now_ms) {
    if (!s_queue) return;
    input_event_t ev;
    while (xQueueReceive(s_queue, &ev, 0) == pdTRUE) {
        ESP_LOGI(TAG, "event %s", input_event_name(ev));
        for (int i = 0; i < s_nsubs; i++) s_subs[i].fn(ev, now_ms, s_subs[i].ctx);
    }
}

int input_source_count(void) {
    return SOURCE_COUNT;
}

void input_arm_wakeup(void) {
    for (int i = 0; i < SOURCE_COUNT; i++) {
        if (SOURCES[i]->arm_wakeup) SOURCES[i]->arm_wakeup();
    }
}
