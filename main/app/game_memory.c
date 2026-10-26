// Memory (Simon says) with the two eyes: watch which eye lights up, each with
// its own note, then repeat the sequence: A for the left eye, B for the right.
// One more step every round, a little faster each time.

#include "app/app_internal.h"

#define EYE_LX 34
#define EYE_RX 94
#define EYE_Y 28
#define EYE_R 14
#define SHOW_ON_START_MS 450
#define SHOW_ON_MIN_MS 220
#define SHOW_GAP_MS 180
#define PRESS_LIT_MS 200
#define GOOD_MS 700
#define FIRST_DELAY_MS 600

static uint32_t on_ms(const memory_t *m) {
    const int ms = SHOW_ON_START_MS - m->len * 20;
    return (uint32_t)(ms < SHOW_ON_MIN_MS ? SHOW_ON_MIN_MS : ms);
}

static void light(app_t *a, int eye, uint32_t now, uint32_t ms) {
    memory_t *m = &a->g.mem;
    m->lit = eye;
    m->lit_until = now + ms;
    app_play(a, eye ? SND_TONE_R : SND_TONE_L);
}

static void show_round(app_t *a, uint32_t now) {
    memory_t *m = &a->g.mem;
    m->phase = MEM_SHOW;
    m->pos = 0;
    m->lit = -1;
    m->t0 = now + FIRST_DELAY_MS;  // when the first step lights
}

static void begin(app_t *a, uint32_t now) {
    memory_t *m = &a->g.mem;
    for (int i = 0; i < MEMORY_MAX; i++) m->seq[i] = (uint8_t)rng_range(&a->rng, 0, 1);
    m->len = 1;
    m->score = 0;
    m->best = false;
    show_round(a, now);
}

void memory_start(app_t *a, uint32_t now) {
    a->g.mem.phase = MEM_START;
    a->g.mem.lit = -1;
    a->g.mem.t0 = now;
}

static void over(app_t *a, uint32_t now) {
    memory_t *m = &a->g.mem;
    m->phase = MEM_OVER;
    m->t0 = now;
    m->best = app_record(a, &a->save.memory_pts, (uint16_t)m->score, false);
    a->mood = m->best ? EXPR_EXCITED : EXPR_SAD;
    app_play(a, m->best ? SND_RECORD : SND_FAIL);
    app_reward(a, 4 + (m->score > 12 ? 12 : m->score), 2, 2 + m->score);
}

bool memory_event(app_t *a, input_event_t ev, uint32_t now) {
    memory_t *m = &a->g.mem;
    if (ev == INPUT_BTN_A_SHORT || ev == INPUT_BTN_A_LONG) return true;
    if (ev == INPUT_BTN_B_SHORT && m->swallow_b) {
        m->swallow_b = false;
        return true;
    }
    if (m->phase == MEM_START || m->phase == MEM_OVER) {
        if (ev != INPUT_BTN_A_PRESS) return false;  // B: back to the menu
        if (m->phase == MEM_START || app_elapsed(now, m->t0, 800)) begin(a, now);
        return true;
    }
    if (ev != INPUT_BTN_A_PRESS && ev != INPUT_BTN_B_PRESS) return false;
    if (ev == INPUT_BTN_B_PRESS) m->swallow_b = true;
    if (m->phase != MEM_INPUT) return true;  // watching: presses do nothing
    const int eye = ev == INPUT_BTN_B_PRESS;
    if (eye != m->seq[m->pos]) {
        m->lit = eye;
        m->lit_until = now + 600;
        over(a, now);
        return true;
    }
    light(a, eye, now, PRESS_LIT_MS);
    if (++m->pos == m->len) {
        m->score = m->len;
        m->phase = MEM_GOOD;
        m->t0 = now;
    }
    return true;
}

void memory_update(app_t *a, uint32_t now) {
    memory_t *m = &a->g.mem;
    if (m->lit >= 0 && app_elapsed(now, m->lit_until, 0)) m->lit = -1;
    switch (m->phase) {
        case MEM_SHOW:
            // Steps light one after another: on for on_ms, then a short gap.
            if (m->lit < 0 && app_elapsed(now, m->t0, 0)) {
                if (m->pos == m->len) {
                    m->phase = MEM_INPUT;
                    m->pos = 0;
                    break;
                }
                light(a, m->seq[m->pos], now, on_ms(m));
                m->pos++;
                m->t0 = now + on_ms(m) + SHOW_GAP_MS;
            }
            break;
        case MEM_GOOD:
            if (app_elapsed(now, m->t0, GOOD_MS)) {
                if (m->len == MEMORY_MAX) {  // perfect: nothing left to remember
                    over(a, now);
                } else {
                    m->len++;
                    show_round(a, now);
                }
            }
            break;
        default:
            break;
    }
}

static void eye(gfx_t *g, int cx, bool lit) {
    gfx_fill_circle(g, cx, EYE_Y, EYE_R, GFX_WHITE);
    if (!lit) gfx_fill_circle(g, cx, EYE_Y, EYE_R - 2, GFX_BLACK);
}

void memory_render(const app_t *a, gfx_t *g) {
    const memory_t *m = &a->g.mem;
    if (m->phase == MEM_START) {
        font_draw_centered(g, 64, 4, "MEMORY", 2, GFX_WHITE);
        gfx_fill_circle(g, 50, 31, 8, GFX_WHITE);
        gfx_fill_circle(g, 78, 31, 8, GFX_WHITE);
        gfx_fill_circle(g, 78, 31, 6, GFX_BLACK);
        font_draw_centered(g, 64, 46, "WATCH THEN A / B", 1, GFX_WHITE);
        app_hint(g, "A:START", "B:BACK");
        return;
    }
    if (m->phase == MEM_OVER && app_elapsed(a->now, m->t0, 600)) {
        app_game_over(g, m->score, m->best, a->save.memory_pts);
        return;
    }
    eye(g, EYE_LX, m->lit == 0);
    eye(g, EYE_RX, m->lit == 1);
    char n[8];
    app_itoa(m->phase == MEM_GOOD ? m->len : m->len - 1, n);
    font_draw_centered(g, 64, 24, n, 1, GFX_WHITE);
    const char *msg = m->phase == MEM_SHOW    ? "WATCH"
                      : m->phase == MEM_INPUT ? "YOUR TURN"
                      : m->phase == MEM_GOOD  ? "GOOD!"
                                              : "OOPS";
    font_draw_centered(g, 64, 52, msg, 1, GFX_WHITE);
}

bool memory_animating(const app_t *a) {
    const memory_t *m = &a->g.mem;
    return m->phase == MEM_SHOW || m->phase == MEM_GOOD || m->lit >= 0 ||
           (m->phase == MEM_OVER && !app_elapsed(a->now, m->t0, 700));
}
