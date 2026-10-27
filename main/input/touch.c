// TTP223 touch pad on PIN_TOUCH: its I/O pin is high while touched (the
// module's default; with its "A" jumper bridged it is low, see
// TOUCH_ACTIVE_LOW).
//
// Like the buttons: a level interrupt that is also the light-sleep wake-up,
// masked by the ISR until poll() has seen the touch end. The pin has the
// internal pull-down, so with no pad fitted it simply never fires.

#include "config.h"

#if FEATURE_TOUCH

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "input/input.h"
#include "input/touch_logic.h"

static const char *TAG = "touch";

#ifndef TOUCH_ACTIVE_LOW
#define TOUCH_ACTIVE_LOW 0
#endif
#define ACTIVE_LEVEL (TOUCH_ACTIVE_LOW ? 0 : 1)
#define INTR_ACTIVE (TOUCH_ACTIVE_LOW ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL)

static touch_t s_touch;
static volatile bool s_masked;
static bool s_ready;

static void IRAM_ATTR on_touch(void *arg) {
    (void)arg;
    gpio_intr_disable(PIN_TOUCH);  // level interrupt: would fire until let go
    s_masked = true;
    input_kick_from_isr();
}

static esp_err_t init(void) {
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_TOUCH,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = TOUCH_ACTIVE_LOW ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = TOUCH_ACTIVE_LOW ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = INTR_ACTIVE,
    };
    touch_init(&s_touch);
    esp_err_t err = gpio_config(&cfg);
    if (err == ESP_OK) err = input_gpio_isr_init();
    if (err == ESP_OK) {
        gpio_sleep_sel_dis(PIN_TOUCH);  // keep the pull through light sleep
        err = gpio_isr_handler_add(PIN_TOUCH, on_touch, NULL);
    }
    if (err == ESP_OK) err = gpio_wakeup_enable(PIN_TOUCH, INTR_ACTIVE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO%d: %s", PIN_TOUCH, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "touch pad on GPIO%d", PIN_TOUCH);
    s_ready = true;
    return ESP_OK;
}

static void poll(uint32_t now_ms) {
    if (!s_ready) return;
    const input_event_t ev = touch_update(&s_touch, gpio_get_level(PIN_TOUCH) == ACTIVE_LEVEL, now_ms);
    if (ev != INPUT_NONE) input_post(ev);
    if (s_masked && !touch_busy(&s_touch)) {
        s_masked = false;
        gpio_intr_enable(PIN_TOUCH);
    }
}

static bool busy(uint32_t now_ms) {
    (void)now_ms;
    // While held for a long pet, 5 fps polling is enough to see the release.
    return !s_touch.held && (s_masked || touch_busy(&s_touch));
}

const input_source_t input_source_touch = {
    .name = "touch",
    .init = init,
    .poll = poll,
    .arm_wakeup = NULL,
    .busy = busy,
};

#endif
