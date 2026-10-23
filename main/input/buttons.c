// Buttons A/B/C on GPIO1/3/5 (to GND, internal pull-ups).
//
// PLACEHOLDER (v0.1): compiled only with FEATURE_BUTTONS=1 so the flag builds,
// but produces no events yet. The real driver is Phase 2.
//
// TODO(phase 2):
//  - configure PIN_BTN_A/B/C as inputs with pull-up, any-edge interrupt;
//  - debounce (~25 ms) in poll(), classify short (<600 ms) vs long press,
//    post INPUT_BTN_x_SHORT / INPUT_BTN_x_LONG;
//  - arm_wakeup(): esp_deep_sleep_enable_gpio_wakeup(BIT(PIN_BTN_A),
//    ESP_GPIO_WAKEUP_GPIO_LOW). GPIO1/3/5 are all in the C3's deep-sleep
//    wake range (GPIO0-5).

#include "config.h"

#if FEATURE_BUTTONS

#include "esp_log.h"
#include "input/input.h"

static esp_err_t init(void) {
    ESP_LOGW("buttons", "placeholder: no events until the Phase 2 driver");
    return ESP_OK;
}

static void poll(uint32_t now_ms) {
    (void)now_ms;
}

const input_source_t input_source_buttons = {
    .name = "buttons",
    .init = init,
    .poll = poll,
    .arm_wakeup = NULL,
};

#endif
