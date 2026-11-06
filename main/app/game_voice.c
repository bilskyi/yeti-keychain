// Dino Roar (ITEM_VOICE): a little dinosaur runs, cacti come at it, and your
// voice makes it jump: roar at it. The louder, the higher; humming on keeps
// it in the air a little longer. One point per cactus. A jumps too, so it
// plays without a mic.
//
// No sounds while playing: the mic is deaf while the buzzer plays, and a
// missed shout would feel unfair. Physics runs in fixed 33 ms ticks, like
// Flappy. Heights are in 1/16 px, up is +.

#include "app/app_internal.h"

#define TICK_MS 33
#define GROUND_Y 54
#define YETI_X 24
#define YETI_W 12
#define YETI_H 12
#define GRAVITY 6
#define FLOAT_GRAVITY 3   // while you keep the sound up
#define LOUD_MIN 20       // loudness that counts as a shout
#define JUMP_VY 40        // the smallest jump...
#define JUMP_VY_LOUD 44   // ...plus this much at full loudness
#define BUTTON_LOUDNESS 60  // A jumps like a medium shout
#define ROCK_W 8
#define ROCK_GAP_MIN 64   // px between rocks
#define ROCK_GAP_MAX 100
#define SPEED_START 22    // 1/16 px per tick
#define SPEED_MAX 40

static void place_rock(app_t *a, int i, int x16) {
    voice_t *v = &a->g.voice;
    v->rock[i].x = x16;
    v->rock[i].h = (int)rng_range(&a->rng, 6, 16 + (v->score > 10 ? 4 : v->score * 4 / 10));
    v->rock[i].passed = false;
}

static int farthest(const voice_t *v) {
    int far = v->rock[0].x;
    for (int k = 1; k < VOICE_ROCKS; k++) {
        if (v->rock[k].x > far) far = v->rock[k].x;
    }
    return far;
}

static void reset(app_t *a, uint32_t now) {
    voice_t *v = &a->g.voice;
    v->y = v->vy = 0;
    v->score = 0;
    v->best = false;
    v->acc = 0;
    v->last = now;
    for (int i = 0; i < VOICE_ROCKS; i++) v->rock[i].x = 0;
    for (int i = 0; i < VOICE_ROCKS; i++) {
        place_rock(a, i, i == 0 ? (GFX_W + 10) * 16
                                : farthest(v) + (int)rng_range(&a->rng, ROCK_GAP_MIN, ROCK_GAP_MAX) * 16);
    }
}

static int loudness(const app_t *a) {
    return a->env && a->env->loudness ? a->env->loudness() : -1;
}

void voice_start(app_t *a, uint32_t now) {
    reset(a, now);
    a->g.voice.phase = VOICE_START;
    a->g.voice.t0 = now;
    a->g.voice.no_mic = loudness(a) < 0;
}

static void jump(voice_t *v, int level) {
    if (v->y > 0) return;  // already in the air
    v->vy = JUMP_VY + JUMP_VY_LOUD * (level > 100 ? 100 : level) / 100;
}

bool voice_event(app_t *a, input_event_t ev, uint32_t now) {
    voice_t *v = &a->g.voice;
    if (ev != INPUT_BTN_A_PRESS) return ev == INPUT_BTN_A_SHORT || ev == INPUT_BTN_A_LONG;
    if (v->phase == VOICE_OVER) {
        if (!app_elapsed(now, v->t0, 800)) return true;  // don't restart by accident
        reset(a, now);
        v->phase = VOICE_START;
        return true;
    }
    if (v->phase == VOICE_START) {
        v->phase = VOICE_PLAY;
        v->last = now;
        return true;
    }
    jump(v, BUTTON_LOUDNESS);
    return true;
}

static bool hits(const voice_t *v, int i) {
    const int rx = v->rock[i].x / 16;
    if (YETI_X + YETI_W / 2 - 2 < rx || YETI_X - YETI_W / 2 + 2 > rx + ROCK_W) return false;
    return v->y / 16 < v->rock[i].h - 1;
}

static void crash(app_t *a, uint32_t now) {
    voice_t *v = &a->g.voice;
    v->phase = VOICE_OVER;
    v->t0 = now;
    v->best = app_record(a, &a->save.voice_pts, (uint16_t)v->score, false);
    a->mood = v->best ? EXPR_EXCITED : EXPR_DIZZY;
    app_play(a, v->best ? SND_RECORD : SND_FAIL);
}

static void tick(app_t *a, uint32_t now) {
    voice_t *v = &a->g.voice;
    const int level = v->level;
    if (level >= LOUD_MIN) jump(v, level);
    if (v->y > 0 || v->vy > 0) {
        v->y += v->vy;
        v->vy -= level >= LOUD_MIN && v->vy < 0 ? FLOAT_GRAVITY : GRAVITY;
        if (v->y <= 0) v->y = v->vy = 0;
    }
    const int speed = SPEED_START + (v->score > 18 ? SPEED_MAX - SPEED_START : v->score);
    for (int i = 0; i < VOICE_ROCKS; i++) {
        v->rock[i].x -= speed;
        if (v->rock[i].x / 16 + ROCK_W < 0) {
            place_rock(a, i, farthest(v) + (int)rng_range(&a->rng, ROCK_GAP_MIN, ROCK_GAP_MAX) * 16);
        }
        if (!v->rock[i].passed && v->rock[i].x / 16 + ROCK_W < YETI_X - YETI_W / 2) {
            v->rock[i].passed = true;
            v->score++;
        }
        if (hits(v, i)) {
            crash(a, now);
            return;
        }
    }
}

void voice_update(app_t *a, uint32_t now) {
    voice_t *v = &a->g.voice;
    const int l = loudness(a);
    v->level = l < 0 ? 0 : l;
    if (v->phase != VOICE_PLAY) return;
    v->acc += now - v->last;
    v->last = now;
    if (v->acc > 10 * TICK_MS) v->acc = 10 * TICK_MS;  // a long stall: don't fast-forward
    while (v->acc >= TICK_MS && v->phase == VOICE_PLAY) {
        v->acc -= TICK_MS;
        tick(a, now);
    }
}

// The dinosaur, feet on `feet`: running, or tucked up in the air, with its
// jaw open and the roar coming out while you are loud.
static void dino(gfx_t *g, int feet, bool air, bool loud, uint32_t now) {
    const int x = YETI_X - 8, top = feet - 15;
    app_dino(g, x, top, air ? 2 : (int)(now / 120 % 2), loud, 1);
    if (loud) app_roar(g, x + 16, top + 5, 2);
}

// A cactus h px tall, a trunk with an arm each side once it is big enough.
static void cactus(gfx_t *g, int x, int h) {
    gfx_fill_rrect(g, x + 2, GROUND_Y - h, 4, h + 1, 1, GFX_WHITE);
    if (h < 8) return;
    const int la = GROUND_Y - h * 2 / 3, ra = GROUND_Y - h / 2;
    gfx_fill_rect(g, x, la - 3, 2, 4, GFX_WHITE);
    gfx_fill_rect(g, x, la, 3, 2, GFX_WHITE);
    gfx_fill_rect(g, x + 6, ra - 3, 2, 4, GFX_WHITE);
    gfx_fill_rect(g, x + 5, ra, 3, 2, GFX_WHITE);
}

// How loud you are right now, on the right edge, with a mark where a jump starts.
static void meter(gfx_t *g, int level) {
    const int h = 40, y0 = 8;
    gfx_rect(g, 122, y0, 5, h, GFX_WHITE);
    const int fill = (h - 2) * (level > 100 ? 100 : level) / 100;
    if (fill > 0) gfx_fill_rect(g, 123, y0 + h - 1 - fill, 3, fill, GFX_WHITE);
    const int mark = y0 + h - 1 - (h - 2) * LOUD_MIN / 100;
    gfx_hline(g, 119, 121, mark, GFX_WHITE);
}

void voice_render(const app_t *a, gfx_t *g) {
    const voice_t *v = &a->g.voice;
    if (v->phase == VOICE_OVER) {
        app_game_over(g, v->score, v->best, a->save.voice_pts);
        return;
    }
    gfx_hline(g, 0, GFX_W - 8, GROUND_Y, GFX_WHITE);
    for (int i = 0; i < VOICE_ROCKS; i++) {
        const int x = v->rock[i].x / 16, h = v->rock[i].h;
        if (x > GFX_W - 8) continue;
        cactus(g, x, h);
    }
    dino(g, GROUND_Y - v->y / 16, v->y > 0, !v->no_mic && v->level >= LOUD_MIN, a->now);
    if (!v->no_mic) meter(g, v->level);
    if (v->phase == VOICE_START) {
        font_draw_centered(g, 64, 8, "ROAR!", 2, GFX_WHITE);
        font_draw_centered(g, 64, 28, v->no_mic ? "NO MIC: A JUMPS" : "LOUDER = HIGHER", 1, GFX_WHITE);
        app_hint(g, "START", NULL, "BACK");
    } else {
        char n[8];
        app_itoa(v->score, n);
        font_draw_centered(g, 64, 3, n, 2, GFX_WHITE);
    }
}

bool voice_animating(const app_t *a) {
    return a->g.voice.phase != VOICE_OVER;
}
