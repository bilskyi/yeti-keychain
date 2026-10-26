// Reaction: wait for NOW! and the beep, then press A as fast as you can.
//
// Timing uses the PRESS event (the moment A goes down, after the 25 ms
// debounce), measured from the frame that first showed NOW!. So a result
// includes the debounce and up to a frame of input latency (~30-60 ms): fair
// between tries, a little slower than a lab test.

#include "app/app_internal.h"

#define WAIT_MIN_MS 1500
#define WAIT_MAX_MS 4500
#define SLOW_MS 2000  // no press this long after NOW!: too slow

void reaction_start(app_t *a, uint32_t now) {
    a->g.rx.phase = RX_READY;
    a->g.rx.t0 = now;
}

static void begin_wait(app_t *a, uint32_t now) {
    reaction_t *r = &a->g.rx;
    r->phase = RX_WAIT;
    r->t0 = now;
    r->go_at = now + rng_range(&a->rng, WAIT_MIN_MS, WAIT_MAX_MS);
}

bool reaction_event(app_t *a, input_event_t ev, uint32_t now) {
    reaction_t *r = &a->g.rx;
    if (ev == INPUT_BTN_A_SHORT || ev == INPUT_BTN_A_LONG) return true;  // the release: ignore
    if (ev != INPUT_BTN_A_PRESS) return false;
    switch (r->phase) {
        case RX_READY:
        case RX_RESULT:
        case RX_EARLY:
        case RX_SLOW:
            begin_wait(a, now);
            break;
        case RX_WAIT:
            r->phase = RX_EARLY;
            r->t0 = now;
            app_play(a, SND_FAIL);
            break;
        case RX_GO: {
            const uint32_t ms = now - r->t0;
            r->ms = (uint16_t)(ms > 9999 ? 9999 : ms);
            r->best = app_record(a, &a->save.reaction_ms, r->ms, true);
            if (r->best) a->mood = EXPR_EXCITED;
            r->phase = RX_RESULT;
            r->t0 = now;
            app_play(a, r->best ? SND_RECORD : SND_POINT);
            app_reward(a, 3, 1, r->best ? 6 : 3);
            break;
        }
    }
    return true;
}

void reaction_update(app_t *a, uint32_t now) {
    reaction_t *r = &a->g.rx;
    if (r->phase == RX_WAIT && app_elapsed(now, r->go_at, 0)) {
        r->phase = RX_GO;
        r->t0 = now;  // timing starts with the frame that shows NOW!
        app_play(a, SND_GO);
    } else if (r->phase == RX_GO && app_elapsed(now, r->t0, SLOW_MS)) {
        r->phase = RX_SLOW;
        r->t0 = now;
        app_play(a, SND_FAIL);
    }
}

static void best_line(const app_t *a, gfx_t *g, int y) {
    if (!a->save.reaction_ms) return;
    char line[16] = "BEST ";
    app_itoa(a->save.reaction_ms, line + 5);
    font_draw_centered(g, 64, y, line, 1, GFX_WHITE);
}

void reaction_render(const app_t *a, gfx_t *g) {
    const reaction_t *r = &a->g.rx;
    switch (r->phase) {
        case RX_READY:
            font_draw_centered(g, 64, 8, "WAIT FOR", 1, GFX_WHITE);
            font_draw_centered(g, 64, 20, "NOW!", 2, GFX_WHITE);
            font_draw_centered(g, 64, 40, "PRESS A", 1, GFX_WHITE);
            best_line(a, g, 54);
            break;
        case RX_WAIT: {
            // A sleepy dot face, breathing: nothing to see yet.
            const int dots = (int)((a->now - r->t0) / 400 % 4);
            gfx_fill_rect(g, 34 - 8, 28, 16, 3, GFX_WHITE);
            gfx_fill_rect(g, 94 - 8, 28, 16, 3, GFX_WHITE);
            for (int i = 0; i < dots; i++) gfx_fill_rect(g, 56 + i * 7, 46, 3, 3, GFX_WHITE);
            break;
        }
        case RX_GO:
            gfx_fill_rect(g, 0, 0, GFX_W, GFX_H, GFX_WHITE);
            font_draw_centered(g, 64, 18, "NOW!", 4, GFX_BLACK);
            break;
        case RX_RESULT: {
            char n[8];
            app_itoa(r->ms, n);
            const int w = font_width(n, 3) + 4 + font_width("MS", 1);
            const int x = 64 - w / 2;
            font_draw(g, x, 12, n, 3, GFX_WHITE);
            font_draw(g, x + font_width(n, 3) + 4, 26, "MS", 1, GFX_WHITE);
            if (r->best) {
                font_draw_centered(g, 64, 42, "NEW BEST!", 1, GFX_WHITE);
            } else {
                best_line(a, g, 42);
            }
            font_draw_centered(g, 64, 54, "A: AGAIN", 1, GFX_WHITE);
            break;
        }
        case RX_EARLY:
            // Angry dots: you jumped the gun.
            gfx_fill_circle(g, 34, 22, 7, GFX_WHITE);
            gfx_fill_circle(g, 94, 22, 7, GFX_WHITE);
            for (int k = 0; k < 3; k++) {
                gfx_line(g, 24, 10 + k, 40, 15 + k, GFX_WHITE);
                gfx_line(g, 104, 10 + k, 88, 15 + k, GFX_WHITE);
            }
            font_draw_centered(g, 64, 38, "TOO SOON!", 2, GFX_WHITE);
            font_draw_centered(g, 64, 56, "A: AGAIN", 1, GFX_WHITE);
            break;
        case RX_SLOW:
            font_draw_centered(g, 64, 16, "TOO SLOW", 2, GFX_WHITE);
            font_draw_centered(g, 64, 44, "A: AGAIN", 1, GFX_WHITE);
            break;
    }
}

bool reaction_animating(const app_t *a) {
    // Fast frames while waiting, so NOW! appears on time, and while it shows.
    return a->g.rx.phase == RX_WAIT || a->g.rx.phase == RX_GO;
}
