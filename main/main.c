// OLED keychain companion, v0.5.
//
// Boot, then one loop: input -> policy -> pet -> face -> render -> flush -> sleep.
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
#include "app/app.h"
#include "led/led.h"
#include "pet/story.h"
#include "storage/storage.h"

static const char *TAG = "main";

static face_t s_face;
static gfx_t s_gfx;
static policy_t s_policy;
static battery_status_t s_battery;
static app_t s_app;
static app_save_t s_save;
static story_t s_story;
static bool s_singing;  // a story song is playing; shake (or any input) stops it

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

// --- what the menu and games need from the hardware ---

static void env_save(const app_save_t *s) {
    s_save = *s;
    storage_save(s);
}

static bool env_tilt(int *mg) {
    int a[3];
    if (!input_accel_mg(a)) return false;
    *mg = ACCEL_TILT_SIGN * a[ACCEL_TILT_AXIS];
    return true;
}

static const app_env_t ENV = {
    .play = sound_fx,
    .save = env_save,
    .mute = sound_set_muted,
    .tilt = env_tilt,
    .accel = input_accel_raw_mg,
    .orient = input_accel_set_orient,
    .song = sound_song,
    .song_stop = sound_stop,
    .song_playing = sound_song_playing,
};

// Each note of a song flashes the LED for half its length.
static void note_flash(uint16_t hz, uint16_t ms) {
    if (hz) led_flash(ms / 2);
}

// Input during a story song: it stops. A shake is the proper way (it gets
// dizzy and offended); anything else gets a disappointed look. True when the
// event was used up stopping it.
static bool stop_song(input_event_t ev, uint32_t now) {
    if (!s_singing || ev == INPUT_BTN_A_PRESS || ev == INPUT_BTN_B_PRESS ||
        ev == INPUT_BTN_C_PRESS || ev == INPUT_TILT_LEFT || ev == INPUT_TILT_RIGHT) {
        return false;
    }
    s_singing = false;
    sound_stop();
    led_pattern(LED_OFF);
    face_fx_stop(&s_face);
    if (ev == INPUT_SHAKE) {
        face_on_event(&s_face, ev, now);  // dizzy, then angry
        sound_fx(SND_HMPH);
    } else {
        face_react(&s_face, EXPR_SAD, now, 2500);
    }
    return true;
}

static bool oriented(void) {
    return s_app.save.orient[0] || s_app.save.orient[1] || s_app.save.orient[2];
}

// The face asked for a sound (picked up, upside down, a sneeze...).
static void play_cue(face_cue_t cue) {
    static const app_sound_t CUES[] = {
        [CUE_HI] = SND_HI,         [CUE_SCARED] = SND_SCARED, [CUE_RELIEF] = SND_DING,
        [CUE_WHEE] = SND_WHEE,     [CUE_SNEEZE] = SND_SNEEZE, [CUE_HUM] = SND_HUM,
    };
    if (cue != CUE_NONE && (unsigned)cue < sizeof(CUES) / sizeof(CUES[0])) sound_fx(CUES[cue]);
}

static void on_input(input_event_t ev, uint32_t now, void *ctx) {
    (void)ctx;
    if (app_active(&s_app)) {
        policy_note_activity(&s_policy, now_ms64());
        app_on_event(&s_app, ev, now);
        return;
    }
    policy_on_event(&s_policy, ev, now_ms64());
    if (stop_song(ev, now)) return;
    if (s_face.fx == FX_ALARM) {  // seen it: hush the siren
        led_pattern(LED_OFF);
        face_fx_stop(&s_face);
    }
    if (ev == INPUT_BTN_B_SHORT) {  // B: the menu
        app_open(&s_app, now);
        return;
    }
    face_on_event(&s_face, ev, now);
    app_face_event(&s_app, ev);
    sound_on_event(ev);
}

// The pet's own events (pet/story.h), when nothing else is going on.
static void run_story(uint32_t now) {
    if (s_singing && !sound_song_playing()) {  // sang it to the end: proud
        s_singing = false;
        face_fx_stop(&s_face);
        face_react(&s_face, EXPR_EXCITED, now, 3000);
        led_pattern(LED_BLINK);
    }
    const bool idle = !app_active(&s_app) && !s_policy.night && !s_singing && s_face.fx == FX_NONE;
    switch (story_update(&s_story, now, app_needs(&s_app), idle, !s_app.save.muted)) {
        case STORY_ALARM:
            ESP_LOGI(TAG, "story: alarm (needs 0x%x)", app_needs(&s_app));
            sound_fx(SND_ALARM);
            led_pattern(LED_SIREN);
            face_fx(&s_face, FX_ALARM, NULL, now, 4000);
            break;
        case STORY_GLITCH: {
            const char *line = story_log(&s_story);
            ESP_LOGI(TAG, "story: glitch \"%s\"", line);
            sound_fx(SND_GLITCH);
            led_pattern(LED_GLITCH);
            face_fx(&s_face, FX_GLITCH, line, now, 1800);
            break;
        }
        case STORY_SONG:
            if (sound_song(story_song(&s_story))) {
                s_singing = true;
                face_fx(&s_face, FX_SING, NULL, now, 0);
            }
            break;
        case STORY_NONE:
            break;
    }
}

void app_main(void) {
    const bool brownout = power_reset_was_brownout();
    power_init();

    if (i2c_bus_init() == ESP_OK) display_init(i2c_bus_handle(), CONTRAST_DAY);

    battery_init();
    s_battery = battery_read();
    if (brownout || s_battery.level == BATTERY_CRITICAL) lowbat_shutdown();

    storage_load(&s_save);
    sound_init();
    sound_set_muted(s_save.muted);
    led_init();
    sound_set_note_hook(note_flash);
    input_init();
    input_accel_set_orient(s_save.orient);
    input_subscribe(on_input, NULL);
    net_init();

    uint32_t now = (uint32_t)now_ms64();
    face_init(&s_face, esp_random(), now);
    policy_init(&s_policy);
    app_init(&s_app, &ENV, &s_save, esp_random());
    story_init(&s_story, esp_random(), now);
    uint64_t next_battery = now_ms64() + BATTERY_SAMPLE_MS;

    ESP_LOGI(TAG, "v0.5 running; buttons=%d accel=%d sound=%d led=%d battery_adc=%d wifi=%d ble=%d",
             FEATURE_BUTTONS, FEATURE_ACCEL, FEATURE_SOUND, FEATURE_LED, FEATURE_BATTERY_ADC,
             FEATURE_WIFI, FEATURE_BLE);
    sound_hello();

    for (;;) {
        const uint64_t now64 = now_ms64();
        now = (uint32_t)now64;

        input_poll(now);
        input_dispatch(now);

        if (policy_update(&s_policy, now64)) {
            ESP_LOGI(TAG, "night mode %s", s_policy.night ? "on" : "off");
            if (s_policy.night) app_close(&s_app);
            face_set_night(&s_face, s_policy.night, now);
            display_set_contrast(policy_contrast(&s_policy));
        }

        if (now64 >= next_battery) {
            next_battery = now64 + BATTERY_SAMPLE_MS;
            s_battery = battery_read();
            if (s_battery.level == BATTERY_CRITICAL) lowbat_shutdown();
        }

        app_tick(&s_app, now, s_policy.night);
        face_set_needs(&s_face, app_needs(&s_app));

        const bool was_app = app_active(&s_app);
        app_update(&s_app, now);
        const int mood = app_take_mood(&s_app);
        if (app_take_sleep(&s_app)) {
            policy_sleep(&s_policy);  // night starts at the next policy_update
            sound_goodnight();
        } else if (was_app && !app_active(&s_app)) {
            // Back from the menu or a game: the face shows how it went.
            face_react(&s_face, mood >= 0 ? (expression_t)mood : EXPR_HAPPY, now, 4000);
        }

        if (s_singing && app_active(&s_app)) stop_song(INPUT_NONE, now);
        run_story(now);

        int mg[3];
        if (!app_active(&s_app) && input_accel_mg(mg)) face_feel(&s_face, mg, oriented(), now);
        face_update(&s_face, now);
        play_cue(face_take_cue(&s_face));
        if (app_active(&s_app)) {
            gfx_clear(&s_gfx);
            gfx_set_offset(&s_gfx, 0, 0);
            app_render(&s_app, &s_gfx);
            display_flush(&s_gfx);
        } else {
            render(now);
        }

        // Fast while something moves (on screen, or a button held / the
        // device moving), slow while still. Subtract the frame's own work so
        // the animation keeps its pace. An input interrupt ends the wait early.
        const bool moving = app_active(&s_app) ? app_is_animating(&s_app) : face_is_animating(&s_face);
        const bool fast = moving || input_busy(now);
        const uint32_t period = fast ? FRAME_MS_ANIMATING : FRAME_MS_IDLE;
        const uint32_t spent = (uint32_t)(now_ms64() - now64);
        input_wait(spent < period ? period - spent : 1);
    }
}
