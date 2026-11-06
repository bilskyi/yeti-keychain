#include "storage/storage.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "storage";

#define NS "kc"
#define KEY "save"
// Bumped when app_save_t changes shape: an old record is then migrated, or
// ignored, rather than read as garbage.
#define VERSION 4

typedef struct {
    uint8_t version;
    app_save_t save;
} record_t;

// Version 3 (v0.7): the same, without Maze's best.
typedef struct {
    uint8_t version;
    struct {
        uint16_t reaction_ms, catch_pts, flappy_pts, snake_pts, voice_pts;
        uint8_t volume, brightness;
        int8_t orient[3];
    } save;
} record_v3_t;

// Version 2 (v0.5-v0.6): scores, mute, calibration and the tamagotchi pet.
typedef struct {
    uint8_t version;
    struct {
        uint16_t reaction_ms, catch_pts, flappy_pts, snake_pts, memory_pts;
        uint8_t muted;
        int8_t orient[3];
        struct {
            uint8_t food, fun, energy, level;
            uint16_t xp;
            uint32_t minutes;
        } pet;
    } save;
} record_v2_t;

static bool s_ready;

static bool init(void) {
    if (s_ready) return true;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Partition full or from another IDF: start it over (only scores lost).
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS unavailable: %s (scores won't be kept)", esp_err_to_name(err));
        return false;
    }
    s_ready = true;
    return true;
}

void storage_load(app_save_t *s) {
    *s = (app_save_t)APP_SAVE_DEFAULTS;
    nvs_handle_t h;
    if (!init() || nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return;
    union {
        record_t now;
        record_v3_t v3;
        record_v2_t v2;
    } r;
    size_t len = sizeof(r);
    if (nvs_get_blob(h, KEY, &r, &len) == ESP_OK) {
        if (len == sizeof(r.now) && r.now.version == VERSION) {
            *s = r.now.save;
        } else if (len == sizeof(r.v3) && r.v3.version == 3) {
            s->reaction_ms = r.v3.save.reaction_ms;
            s->catch_pts = r.v3.save.catch_pts;
            s->flappy_pts = r.v3.save.flappy_pts;
            s->snake_pts = r.v3.save.snake_pts;
            s->voice_pts = r.v3.save.voice_pts;
            s->volume = r.v3.save.volume;
            s->brightness = r.v3.save.brightness;
            memcpy(s->orient, r.v3.save.orient, sizeof(s->orient));
            ESP_LOGI(TAG, "migrated a v3 record");
        } else if (len == sizeof(r.v2) && r.v2.version == 2) {
            // Keep the best scores and the calibration; sound off stays off.
            s->reaction_ms = r.v2.save.reaction_ms;
            s->catch_pts = r.v2.save.catch_pts;
            s->flappy_pts = r.v2.save.flappy_pts;
            s->snake_pts = r.v2.save.snake_pts;
            if (r.v2.save.muted) s->volume = 0;
            memcpy(s->orient, r.v2.save.orient, sizeof(s->orient));
            ESP_LOGI(TAG, "migrated a v2 record");
        }
        ESP_LOGI(TAG, "loaded: volume %u, brightness %u, %s", s->volume, s->brightness,
                 s->orient[0] || s->orient[1] || s->orient[2] ? "calibrated" : "not calibrated");
    }
    nvs_close(h);
}

void storage_save(const app_save_t *s) {
    nvs_handle_t h;
    if (!init() || nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    const record_t r = {.version = VERSION, .save = *s};
    esp_err_t err = nvs_set_blob(h, KEY, &r, sizeof(r));
    if (err == ESP_OK) err = nvs_commit(h);
    if (err != ESP_OK) ESP_LOGW(TAG, "save failed: %s", esp_err_to_name(err));
    nvs_close(h);
}
