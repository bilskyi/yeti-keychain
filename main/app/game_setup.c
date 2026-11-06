// Calibration: teach it which way is up, in two steps (lay it flat, hold it
// upright), so tilt and face-down mean the same whichever way the board sits
// in the case.

#include "app/app_internal.h"
#include "input/orient.h"

// ---------------------------------------------------------------------------
// Setup: calibrate the accelerometer in two steps.
// ---------------------------------------------------------------------------

void setup_start(app_t *a, uint32_t now) {
    a->g.setup.phase = SETUP_FLAT;
    a->g.setup.t0 = now;
}

bool setup_event(app_t *a, input_event_t ev, uint32_t now) {
    setup_t *s = &a->g.setup;
    if (ev != INPUT_BTN_A_SHORT) return false;
    int mg[3] = {0, 0, 0};
    const bool ok = a->env && a->env->accel && a->env->accel(mg);
    s->t0 = now;
    switch (s->phase) {
        case SETUP_FLAT: {
            // Must already be clearly along one axis (orient_solve checks again).
            bool clear = false;
            for (int i = 0; i < 3; i++) {
                s->flat[i] = mg[i];
                if (mg[i] > 700 || mg[i] < -700) clear = true;
            }
            s->phase = ok && clear ? SETUP_UPRIGHT : SETUP_FAIL;
            app_play(a, s->phase == SETUP_UPRIGHT ? SND_SELECT : SND_FAIL);
            break;
        }
        case SETUP_UPRIGHT: {
            int8_t map[3];
            if (ok && orient_solve(s->flat, mg, map)) {
                for (int i = 0; i < 3; i++) a->save.orient[i] = map[i];
                if (a->env->orient) a->env->orient(map);
                if (a->env->save) a->env->save(&a->save);
                s->phase = SETUP_DONE;
                app_play(a, SND_RECORD);
            } else {
                s->phase = SETUP_FAIL;
                app_play(a, SND_FAIL);
            }
            break;
        }
        case SETUP_DONE:
        case SETUP_FAIL:
            s->phase = SETUP_FLAT;
            app_play(a, SND_SELECT);
            break;
    }
    return true;
}

void setup_update(app_t *a, uint32_t now) {
    (void)a;
    (void)now;
}

void setup_render(const app_t *a, gfx_t *g) {
    const setup_t *s = &a->g.setup;
    switch (s->phase) {
        case SETUP_FLAT:
            font_draw_centered(g, 64, 2, "1/2", 1, GFX_WHITE);
            font_draw_centered(g, 64, 16, "LAY ME FLAT", 1, GFX_WHITE);
            font_draw_centered(g, 64, 26, "SCREEN UP", 1, GFX_WHITE);
            gfx_fill_rect(g, 40, 40, 48, 4, GFX_WHITE);
            app_hint(g, "OK", NULL, "BACK");
            break;
        case SETUP_UPRIGHT:
            font_draw_centered(g, 64, 2, "2/2", 1, GFX_WHITE);
            font_draw_centered(g, 64, 16, "HOLD ME UPRIGHT", 1, GFX_WHITE);
            font_draw_centered(g, 64, 26, "LIKE NORMAL", 1, GFX_WHITE);
            gfx_fill_rect(g, 62, 36, 4, 14, GFX_WHITE);
            app_hint(g, "OK", NULL, "BACK");
            break;
        case SETUP_DONE: {
            font_draw_centered(g, 64, 4, "DONE!", 2, GFX_WHITE);
            font_draw_centered(g, 64, 24, "TILT ME", 1, GFX_WHITE);
            // A spirit level to check it: the bubble goes to the high side.
            int mg = 0;
            if (a->env && a->env->tilt) a->env->tilt(&mg);
            if (mg > 500) mg = 500;
            if (mg < -500) mg = -500;
            gfx_fill_rrect(g, 14, 36, 100, 14, 7, GFX_WHITE);
            gfx_fill_rrect(g, 16, 38, 96, 10, 5, GFX_BLACK);
            gfx_fill_rect(g, 56, 38, 1, 10, GFX_WHITE);
            gfx_fill_rect(g, 72, 38, 1, 10, GFX_WHITE);
            gfx_fill_circle(g, 64 - mg * 42 / 500, 42, 4, GFX_WHITE);
            app_hint(g, "AGAIN", NULL, "BACK");
            break;
        }
        case SETUP_FAIL:
            font_draw_centered(g, 64, 8, "HMM...", 2, GFX_WHITE);
            font_draw_centered(g, 64, 28, "HOLD STILL AND", 1, GFX_WHITE);
            font_draw_centered(g, 64, 38, "TRY AGAIN", 1, GFX_WHITE);
            app_hint(g, "AGAIN", NULL, "BACK");
            break;
    }
}

bool setup_animating(const app_t *a) {
    return a->g.setup.phase == SETUP_DONE;
}
