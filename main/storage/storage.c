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
#define VERSION 2

typedef struct {
    uint8_t version;
    app_save_t save;
} record_t;

// Version 1 (v0.3): best scores and mute only.
typedef struct {
    uint8_t version;
    struct {
        uint16_t reaction_ms, catch_pts, flappy_pts;
        uint8_t muted;
    } save;
} record_v1_t;

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
    memset(s, 0, sizeof(*s));
    nvs_handle_t h;
    if (!init() || nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return;
    record_t r;
    size_t len = sizeof(r);
    if (nvs_get_blob(h, KEY, &r, &len) == ESP_OK) {
        const record_v1_t *v1 = (const record_v1_t *)&r;
        if (len == sizeof(r) && r.version == VERSION) {
            *s = r.save;
        } else if (len == sizeof(record_v1_t) && v1->version == 1) {
            s->reaction_ms = v1->save.reaction_ms;  // keep the old best scores
            s->catch_pts = v1->save.catch_pts;
            s->flappy_pts = v1->save.flappy_pts;
            s->muted = v1->save.muted;
            ESP_LOGI(TAG, "migrated a v1 record");
        }
        ESP_LOGI(TAG, "loaded: pet lv %u food %u fun %u energy %u age %lu min; muted %u",
                 s->pet.level, s->pet.food, s->pet.fun, s->pet.energy,
                 (unsigned long)s->pet.minutes, s->muted);
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
