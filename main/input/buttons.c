// Buttons A/B/C on GPIO1/3/5 (to GND, internal pull-ups).
//
// Each pin has a low-level interrupt that is also its light-sleep wake-up.
// The ISR only masks its own pin and kicks the main loop; poll() then samples
// the pins every frame (fast frames while busy) and debounces in
// button_logic.c. Once a button is released and settled, its interrupt is
// unmasked again. A level interrupt rather than an edge, because only level
// works as a GPIO wake-up from light sleep.
//
// Deep sleep: button A wakes the chip (GPIO0-5 are the C3's deep-sleep wake
// pins; IDF keeps the pull-up on during deep sleep for a LOW wake pin).

#include "config.h"

#if FEATURE_BUTTONS

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "input/button_logic.h"
#include "input/input.h"

static const char *TAG = "buttons";

static const struct {
    gpio_num_t pin;
    input_event_t ev_short, ev_long;
} BUTTONS[] = {
    {PIN_BTN_A, INPUT_BTN_A_SHORT, INPUT_BTN_A_LONG},
    {PIN_BTN_B, INPUT_BTN_B_SHORT, INPUT_BTN_B_LONG},
    {PIN_BTN_C, INPUT_BTN_C_SHORT, INPUT_BTN_C_LONG},
};
#define NBUTTONS ((int)(sizeof(BUTTONS) / sizeof(BUTTONS[0])))

static button_t s_btn[NBUTTONS];
static volatile bool s_masked[NBUTTONS];
static bool s_ready;

static void IRAM_ATTR on_edge(void *arg) {
    const int i = (int)(intptr_t)arg;
    gpio_intr_disable(BUTTONS[i].pin);  // level interrupt: would fire until release
    s_masked[i] = true;
    input_kick_from_isr();
}

static esp_err_t init(void) {
    uint64_t mask = 0;
    for (int i = 0; i < NBUTTONS; i++) mask |= 1ULL << BUTTONS[i].pin;
    const gpio_config_t cfg = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_LOW_LEVEL,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err == ESP_OK) err = input_gpio_isr_init();
    for (int i = 0; i < NBUTTONS && err == ESP_OK; i++) {
        button_init(&s_btn[i]);
        // Keep the pull-up through light sleep instead of IDF's sleep config.
        gpio_sleep_sel_dis(BUTTONS[i].pin);
        err = gpio_isr_handler_add(BUTTONS[i].pin, on_edge, (void *)(intptr_t)i);
        if (err == ESP_OK) err = gpio_wakeup_enable(BUTTONS[i].pin, GPIO_INTR_LOW_LEVEL);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "setup failed: %s", esp_err_to_name(err));
        return err;
    }
    s_ready = true;
    return ESP_OK;
}

static void poll(uint32_t now_ms) {
    if (!s_ready) return;
    for (int i = 0; i < NBUTTONS; i++) {
        const bool pressed = gpio_get_level(BUTTONS[i].pin) == 0;
        switch (button_update(&s_btn[i], pressed, now_ms)) {
            case BUTTON_SHORT: input_post(BUTTONS[i].ev_short); break;
            case BUTTON_LONG: input_post(BUTTONS[i].ev_long); break;
            case BUTTON_NONE: break;
        }
        if (s_masked[i] && !button_busy(&s_btn[i])) {
            s_masked[i] = false;
            gpio_intr_enable(BUTTONS[i].pin);
        }
    }
}

static bool busy(uint32_t now_ms) {
    (void)now_ms;
    for (int i = 0; i < NBUTTONS; i++) {
        if (s_masked[i] || button_busy(&s_btn[i])) return true;
    }
    return false;
}

static void arm_wakeup(void) {
    if (!s_ready) return;
    esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_BTN_A, ESP_GPIO_WAKEUP_GPIO_LOW);
}

const input_source_t input_source_buttons = {
    .name = "buttons",
    .init = init,
    .poll = poll,
    .arm_wakeup = arm_wakeup,
    .busy = busy,
};

#endif
