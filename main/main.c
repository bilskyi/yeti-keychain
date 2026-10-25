// OLED keychain companion, v0.2.
//
// Boot, then one loop: input -> policy -> face -> render -> flush -> sleep.
// The loop blocks in input_wait() between frames, which is when the C3 drops
// into automatic light sleep.

#include <inttypes.h>

#include "config.h"
#include "display/display.h"
#include "display/gfx.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "face/face.h"
#include "face/hud.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "input/input.h"
#include "net/net.h"
#include "power/battery.h"
#include "power/policy.h"
#include "power/power.h"
#include "sound/sound.h"

static const char *TAG = "main";

static face_t s_face;
static gfx_t s_gfx;
static policy_t s_policy;
static battery_status_t s_battery;

void app_main(void);

static uint64_t now_ms64(void) {
    return (uint64_t)esp_timer_get_time() / 1000;
}

static void render(uint32_t now) {
    int dx, dy;
    policy_burnin_offset(now_ms64(), &dx, &dy);
    gfx_clear(&s_gfx);
    gfx_set_offset(&s_gfx, dx, dy);
    face_render(&s_face, &s_gfx);
    hud_battery(&s_gfx, &s_battery, now);  // shifted too: it is static, so most at risk
    display_flush(&s_gfx);
}

// Battery is (nearly) empty: show it, then deep sleep rather than let the
// next brownout reset us into a boot loop.
static void lowbat_shutdown(void) {
    ESP_LOGW(TAG, "battery low: showing low-battery face, then deep sleep");
    display_set_contrast(CONTRAST_LOWBAT);
    uint32_t now = (uint32_t)now_ms64();
    face_init(&s_face, esp_random(), now);
    s_face.expr = EXPR_LOWBAT;
    s_face.expr_until = now + LOWBAT_FACE_MS * 2;  // the mood machine must not take over
    const uint32_t until = now + LOWBAT_FACE_MS;
    while ((int32_t)(until - now) > 0) {
        face_update(&s_face, now);
        render(now);
        vTaskDelay(pdMS_TO_TICKS(FRAME_MS_IDLE));
        now = (uint32_t)now_ms64();
    }
    power_deep_sleep(LOWBAT_RETRY_MS);
}

static void on_input(input_event_t ev, uint32_t now, void *ctx) {
    (void)ctx;
    policy_on_event(&s_policy, ev, now_ms64());
    face_on_event(&s_face, ev, now);
    sound_on_event(ev);
}

void app_main(void) {
    const bool brownout = power_reset_was_brownout();
    power_init();

    if (i2c_bus_init() == ESP_OK) display_init(i2c_bus_handle(), CONTRAST_DAY);

    battery_init();
    s_battery = battery_read();
    if (brownout || s_battery.level == BATTERY_CRITICAL) lowbat_shutdown();

    sound_init();
    input_init();
    input_subscribe(on_input, NULL);
    net_init();

    uint32_t now = (uint32_t)now_ms64();
    face_init(&s_face, esp_random(), now);
    policy_init(&s_policy);
    uint64_t next_battery = now_ms64() + BATTERY_SAMPLE_MS;

    ESP_LOGI(TAG, "v0.2 running; buttons=%d accel=%d sound=%d battery_adc=%d wifi=%d ble=%d",
             FEATURE_BUTTONS, FEATURE_ACCEL, FEATURE_SOUND, FEATURE_BATTERY_ADC, FEATURE_WIFI,
             FEATURE_BLE);
    sound_hello();

    for (;;) {
        const uint64_t now64 = now_ms64();
        now = (uint32_t)now64;

        input_poll(now);
        input_dispatch(now);

        if (policy_update(&s_policy, now64)) {
            ESP_LOGI(TAG, "night mode %s", s_policy.night ? "on" : "off");
            face_set_night(&s_face, s_policy.night, now);
            display_set_contrast(policy_contrast(&s_policy));
        }

        if (now64 >= next_battery) {
            next_battery = now64 + BATTERY_SAMPLE_MS;
            s_battery = battery_read();
            if (s_battery.level == BATTERY_CRITICAL) lowbat_shutdown();
        }

        face_update(&s_face, now);
        render(now);

        // Fast while something moves (on screen, or a button held / the
        // device moving), slow while still. Subtract the frame's own work so
        // the animation keeps its pace. An input interrupt ends the wait early.
        const bool fast = face_is_animating(&s_face) || input_busy(now);
        const uint32_t period = fast ? FRAME_MS_ANIMATING : FRAME_MS_IDLE;
        const uint32_t spent = (uint32_t)(now_ms64() - now64);
        input_wait(spent < period ? period - spent : 1);
    }
}
