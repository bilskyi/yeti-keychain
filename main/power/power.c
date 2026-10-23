#include "power/power.h"

#include <inttypes.h>

#include "config.h"
#include "display/display.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "input/input.h"

static const char *TAG = "power";

void power_init(void) {
    const esp_pm_config_t cfg = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 40,  // XTAL; DFS drops here when nothing needs more
        .light_sleep_enable = true,
    };
    const esp_err_t err = esp_pm_configure(&cfg);
    if (err != ESP_OK) {
        // Still works, just draws more: the CPU idles at 80 MHz instead of sleeping.
        ESP_LOGW(TAG, "power management unavailable: %s", esp_err_to_name(err));
    }
}

bool power_reset_was_brownout(void) {
    return esp_reset_reason() == ESP_RST_BROWNOUT;
}

void power_deep_sleep(uint32_t timer_wake_ms) {
    display_power(false);
    input_arm_wakeup();
    if (timer_wake_ms) esp_sleep_enable_timer_wakeup((uint64_t)timer_wake_ms * 1000);
    if (input_source_count() == 0 && !timer_wake_ms) {
        ESP_LOGW(TAG, "no wake source: sleeping until power is cycled");
    }
    ESP_LOGI(TAG, "deep sleep (timer %" PRIu32 " ms)", timer_wake_ms);
    esp_deep_sleep_start();
}
