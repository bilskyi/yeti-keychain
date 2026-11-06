// UPDATE (the settings list): a new firmware over WiFi. Opening it puts up
// the keychain's own WiFi and says how to reach it; leaving it takes the WiFi
// down. The work is net/update.h's; this is the screen.

#include "app/app_internal.h"

#include <string.h>

#define IDLE_MS (10UL * 60UL * 1000UL)  // nobody came: the WiFi goes

static app_update_t status(const app_t *a) {
    app_update_t u = {0};
    if (a->env && a->env->update_status) a->env->update_status(&u);
    return u;
}

void update_start(app_t *a, uint32_t now) {
    a->g.upd.t0 = now;
    a->g.upd.started = a->env && a->env->update_begin && a->env->update_begin();
    if (!a->g.upd.started) app_play(a, SND_FAIL);
}

void update_leave(app_t *a) {
    if (a->g.upd.started && a->env && a->env->update_end) a->env->update_end();
    a->g.upd.started = false;
}

bool update_event(app_t *a, input_event_t ev, uint32_t now) {
    (void)now;
    // A failed upload: A puts the page up for another go.
    if (ev == INPUT_BTN_A_SHORT && status(a).phase == APP_UPD_FAILED) {
        update_leave(a);
        update_start(a, now);
        return true;
    }
    if (ev == INPUT_BTN_C_SHORT) update_leave(a);  // and back, as ever
    return false;
}

void update_update(app_t *a, uint32_t now) {
    const app_update_t u = status(a);
    if (u.phase == APP_UPD_RECEIVING || u.phase == APP_UPD_JOINED) a->g.upd.t0 = now;
    if (a->g.upd.started && u.phase != APP_UPD_DONE && app_elapsed(now, a->g.upd.t0, IDLE_MS)) {
        update_leave(a);
        app_close(a);
    }
}

void update_render(const app_t *a, gfx_t *g) {
    if (!a->g.upd.started) {
        font_draw_centered(g, 64, 20, "NO WIFI", 2, GFX_WHITE);
        app_hint(g, NULL, NULL, "BACK");
        return;
    }
    const app_update_t u = status(a);
    char line[32];
    switch (u.phase) {
        case APP_UPD_WAITING:
        case APP_UPD_JOINED:
            font_draw(g, 0, 0, "WIFI", 1, GFX_WHITE);
            font_draw(g, 30, 0, u.ssid, 1, GFX_WHITE);
            font_draw(g, 0, 11, "PASS", 1, GFX_WHITE);
            font_draw(g, 30, 9, u.pass, 2, GFX_WHITE);
            font_draw(g, 0, 28, "OPEN", 1, GFX_WHITE);
            font_draw(g, 30, 28, u.addr, 1, GFX_WHITE);
            font_draw_centered(g, 64, 42,
                               u.phase == APP_UPD_JOINED ? "CONNECTED: SEND .BIN" : "JOIN FROM PHONE/PC", 1,
                               GFX_WHITE);
            app_hint(g, NULL, NULL, "CANCEL");
            break;
        case APP_UPD_RECEIVING:
            font_draw_centered(g, 64, 6, "UPDATING", 2, GFX_WHITE);
            gfx_rect(g, 8, 28, 112, 10, GFX_WHITE);
            gfx_fill_rect(g, 10, 30, 108 * u.pct / 100, 6, GFX_WHITE);
            app_itoa(u.pct, line);
            strcat(line, "%");
            font_draw_centered(g, 64, 44, line, 1, GFX_WHITE);
            app_hint(g, NULL, NULL, "STOP");
            break;
        case APP_UPD_DONE:
            font_draw_centered(g, 64, 14, "DONE!", 2, GFX_WHITE);
            font_draw_centered(g, 64, 36, "RESTARTING", 1, GFX_WHITE);
            break;
        case APP_UPD_FAILED:
            font_draw_centered(g, 64, 10, "FAILED", 2, GFX_WHITE);
            font_draw_centered(g, 64, 32, u.why ? u.why : "?", 1, GFX_WHITE);
            app_hint(g, "RETRY", NULL, "BACK");
            break;
        default:
            break;
    }
}

bool update_animating(const app_t *a) {
    (void)a;
    return true;  // the screen follows the upload as it comes
}
