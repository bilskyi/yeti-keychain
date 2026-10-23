// LIS3DH accelerometer on the shared I2C bus, INT1 on GPIO0.
//
// PLACEHOLDER (v0.1): compiled only with FEATURE_ACCEL=1 so the flag builds,
// but produces no events yet. The real driver is Phase 2.
//
// TODO(phase 2):
//  - probe 0x18/0x19 on i2c_bus_handle(), check WHO_AM_I (0x0F) == 0x33;
//  - low-power mode at a low ODR, click (TAP/DOUBLE_TAP) detection routed to
//    INT1, read orientation for TILT_LEFT/RIGHT, a magnitude threshold for SHAKE;
//  - arm_wakeup(): keep the click interrupt on INT1 and
//    esp_deep_sleep_enable_gpio_wakeup(BIT(PIN_ACCEL_INT1), level per INT polarity).
//    GPIO0 is in the C3's deep-sleep wake range (GPIO0-5).

#include "config.h"

#if FEATURE_ACCEL

#include "esp_log.h"
#include "input/input.h"

static esp_err_t init(void) {
    ESP_LOGW("lis3dh", "placeholder: no events until the Phase 2 driver");
    return ESP_OK;
}

static void poll(uint32_t now_ms) {
    (void)now_ms;
}

const input_source_t input_source_accel = {
    .name = "lis3dh",
    .init = init,
    .poll = poll,
    .arm_wakeup = NULL,
};

#endif
