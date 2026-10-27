// OLED keychain companion, v0.6.
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
#include "sound/rtttl.h"
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
static bool s_petted;   // the touch pad is held: it purrs
static uint32_t s_next_purr;

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

// Each note of a song (or of its babbled answer) flashes the LED for half its length.
static void note_flash(uint16_t hz, uint16_t ms) {
    if (hz) led_flash(ms / 2);
}

// Input during a story song: it stops. A shake is the proper way (it gets
// dizzy and offended); anything else gets a disappointed look. True when the
// event was used up stopping it.
static bool stop_song(input_event_t ev, uint32_t now) {
    // Pats, cuddles and the mic (deaf while singing anyway) don't stop it.
    if (!s_singing || ev == INPUT_BTN_A_PRESS || ev == INPUT_BTN_B_PRESS ||
        ev == INPUT_BTN_C_PRESS || ev == INPUT_TILT_LEFT || ev == INPUT_TILT_RIGHT ||
        (ev >= INPUT_TOUCH && ev <= INPUT_TALK)) {
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

// The face asked for a sound and a light (picked up, upside down, a sneeze,
// shaken too much, petted for long, hung...).
static void play_cue(face_cue_t cue) {
    static const struct {
        int8_t snd, led;  // -1: none
    } CUES[] = {
        [CUE_NONE] = {-1, -1},
        [CUE_HI] = {SND_HI, LED_DOUBLE},
        [CUE_SCARED] = {SND_SCARED, LED_SCARED},
        [CUE_RELIEF] = {SND_DING, LED_BLINK},
        [CUE_WHEE] = {SND_WHEE, LED_GIGGLE},
        [CUE_SNEEZE] = {SND_SNEEZE, LED_SNEEZE},
        [CUE_HUM] = {SND_HUM, -1},
        [CUE_GRUMBLE] = {SND_GROWL, LED_DOUBLE},
        [CUE_FURY] = {SND_FURY, LED_FURY},
        [CUE_KO] = {SND_KO, LED_KO},
        [CUE_HMPH] = {SND_HMPH, LED_BLINK},
        [CUE_FORGIVEN] = {SND_AWW, LED_DOUBLE},
        [CUE_BLISS] = {-1, LED_BREATHE},
        [CUE_MELT] = {SND_SIGH, LED_DOZE},
        [CUE_OVERLOAD] = {-1, -1},  // the heartbeat below
        [CUE_HEARTS] = {SND_SPARKLE, LED_BURST},
        [CUE_BUZZ] = {SND_BUZZ, -1},
        [CUE_SNOW] = {SND_TWINKLE, LED_TWINKLE},
        [CUE_ROFL] = {SND_ROFL, LED_GIGGLE},
        [CUE_WIND] = {SND_WIND, LED_SCARED},
        [CUE_STRETCH] = {SND_STRETCH, LED_RISE},
        [CUE_JOLT] = {SND_ZAP, LED_BLINK},
        [CUE_ERROR] = {SND_ERROR, LED_DOUBLE},
        [CUE_REBOOT] = {SND_POWERDOWN, LED_BOOT},
        [CUE_BOOTED] = {SND_BOOT, LED_GIGGLE},
    };
    if (cue == CUE_NONE || (unsigned)cue >= sizeof(CUES) / sizeof(CUES[0])) return;
    if (CUES[cue].snd >= 0) sound_fx((app_sound_t)CUES[cue].snd);
    if (CUES[cue].led >= 0) led_pattern((led_pattern_t)CUES[cue].led);
}

// A blink for the small things; the big moments come as face cues.
static void led_on_event(input_event_t ev) {
    switch (ev) {
        case INPUT_TAP:
        case INPUT_CLAP:
        case INPUT_SHAKE:  // a second shake in a row gets the face's cue instead
        case INPUT_TOUCH_END: led_pattern(LED_BLINK); break;
        case INPUT_DOUBLE_TAP: led_pattern(LED_DOUBLE); break;
        case INPUT_TICKLE: led_pattern(LED_GIGGLE); break;
        case INPUT_TOUCH:
        case INPUT_BTN_A_SHORT: led_flash(40); break;
        case INPUT_TOUCH_HOLD: led_pattern(LED_BREATHE); break;
        default: break;
    }
}

static void on_input(input_event_t ev, uint32_t now, void *ctx) {
    (void)ctx;
    if (app_active(&s_app)) {
        policy_note_activity(&s_policy, now_ms64());
        app_on_event(&s_app, ev, now);
        return;
    }
    policy_on_event(&s_policy, ev, now_ms64());
    if (face_hung(&s_face)) {  // frozen: only the face listens (a shake fixes it)
        if (ev == INPUT_TOUCH_END) s_petted = false;
        face_on_event(&s_face, ev, now);
        return;
    }
    if (stop_song(ev, now)) return;
    if (s_face.fx == FX_ALARM) {  // seen it: hush the siren
        led_pattern(LED_OFF);
        face_fx_stop(&s_face);
    }
    if (ev == INPUT_BTN_B_SHORT) {  // B: the menu
        app_open(&s_app, now);
        return;
    }
    // Taps while laughing: giggles, never cutting off the one playing.
    const bool giggling = face_laughing(&s_face) && (ev == INPUT_TOUCH || ev == INPUT_TICKLE);
    face_on_event(&s_face, ev, now);
    app_face_event(&s_app, ev);
    if (!giggling) {
        sound_on_event(ev);
    } else if (!sound_busy()) {
        sound_on_event(INPUT_TICKLE);
    }
    led_on_event(ev);
    switch (ev) {
        case INPUT_TOUCH_HOLD:
            s_petted = true;
            s_next_purr = now + TOUCH_PURR_EVERY_MS;
            break;
        case INPUT_TOUCH_END:
            s_petted = false;
            break;
        case INPUT_TALK:
            sound_babble(input_mic_syllables());
            face_talk(&s_face, input_mic_syllables(), now);
            break;
        case INPUT_DOUBLE_CLAP:  // clap-clap: it sings
            if (!s_singing && sound_song((int)(esp_random() % (uint32_t)songs_count()))) {
                s_singing = true;
                face_fx(&s_face, FX_SING, NULL, now, 0);
            }
            break;
        default:
            break;
    }
}

// The pet's own events (pet/story.h), when nothing else is going on.
static void run_story(uint32_t now) {
    if (s_singing && !sound_song_playing()) {  // sang it to the end: proud
        s_singing = false;
        face_fx_stop(&s_face);
        face_react(&s_face, EXPR_EXCITED, now, 3000);
        led_pattern(LED_BLINK);
    }
    const bool idle = !app_active(&s_app) && !s_policy.night && !s_singing && !s_petted &&
                      s_face.fx == FX_NONE;
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
        case STORY_HANG:  // frozen until shaken
            ESP_LOGI(TAG, "story: hang");
            sound_fx(SND_GLITCH);
            led_pattern(LED_STUCK);
            face_hang(&s_face, now);
            break;
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
    input_mic_set_deaf_check(sound_busy);
    input_accel_set_orient(s_save.orient);
    input_subscribe(on_input, NULL);
    net_init();

    uint32_t now = (uint32_t)now_ms64();
    face_init(&s_face, esp_random(), now);
    face_boot(&s_face, now);
    led_pattern(LED_RISE);
    policy_init(&s_policy);
    app_init(&s_app, &ENV, &s_save, esp_random());
    story_init(&s_story, esp_random(), now);
    uint64_t next_battery = now_ms64() + BATTERY_SAMPLE_MS;

    ESP_LOGI(TAG, "v0.6 running; buttons=%d accel=%d sound=%d led=%d touch=%d mic=%d battery_adc=%d wifi=%d ble=%d",
             FEATURE_BUTTONS, FEATURE_ACCEL, FEATURE_SOUND, FEATURE_LED, FEATURE_TOUCH, FEATURE_MIC,
             FEATURE_BATTERY_ADC, FEATURE_WIFI, FEATURE_BLE);
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
        // Ears on while awake on the face (the mic's ADC keeps the chip out of light sleep).
        input_mic_listen(!s_policy.night && !app_active(&s_app));
        const int stage = s_face.petting ? s_face.pet_stage : -1;
        const uint32_t beat = face_heartbeat_ms(&s_face);
        if (beat && (int32_t)(s_next_purr - now) > (int32_t)beat) s_next_purr = now;  // starts at once
        if (s_petted && !s_singing && (int32_t)(now - s_next_purr) >= 0) {
            if (beat) {  // heart overload: a heartbeat, ever faster, the LED with it
                s_next_purr = now + beat;
                sound_fx(SND_HEARTBEAT);
                led_pattern(LED_BEAT);
            } else {  // a purr; slower while melting
                s_next_purr = now + TOUCH_PURR_EVERY_MS * (stage == 2 ? 2 : 1);
                sound_fx(SND_PURR);
            }
        }
        // Loops a blink cut short come back: breathing while petted, the stuck
        // flicker while hung.
        if (!led_busy()) {
            if (s_face.fx == FX_HANG) {
                led_pattern(LED_STUCK);
            } else if (s_petted && stage != 3 && s_face.fx != FX_HEARTS) {
                led_pattern(stage == 2 ? LED_DOZE : LED_BREATHE);
            }
        }
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
