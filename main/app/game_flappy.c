// Flappy Yeti: press A to flap, fly through the gaps between icicles and
// snowdrifts. One point per gap.
//
// Physics runs in fixed 33 ms ticks whatever the frame rate, so the game
// plays the same when a frame is late. Positions are in 1/16 px.

#include "app/app_internal.h"

#define TICK_MS 33
#define BIRD_X 30
#define BIRD_R 5
#define GRAVITY 7      // 1/16 px per tick^2
#define FLAP_VY (-52)  // 1/16 px per tick
#define MAX_VY 80
#define PIPE_W 10
#define PIPE_GAP 26     // px
#define PIPE_SPACING 56  // px between pipes
#define PIPE_SPEED 24    // 1/16 px per tick (1.5 px)

static void place_pipe(app_t *a, int i, int x16) {
    flappy_t *f = &a->g.fl;
    f->pipe[i].x = x16;
    f->pipe[i].gap = (int)rng_range(&a->rng, 8 + PIPE_GAP / 2, GFX_H - 8 - PIPE_GAP / 2);
    f->pipe[i].passed = false;
}

static void reset(app_t *a, uint32_t now) {
    flappy_t *f = &a->g.fl;
    f->y = 28 * 16;
    f->vy = 0;
    f->score = 0;
    f->best = false;
    f->acc = 0;
    f->last = now;
    for (int i = 0; i < FLAPPY_PIPES; i++) place_pipe(a, i, (GFX_W + 20 + i * PIPE_SPACING) * 16);
}

void flappy_start(app_t *a, uint32_t now) {
    reset(a, now);
    a->g.fl.phase = FLAPPY_START;
    a->g.fl.t0 = now;
}

bool flappy_event(app_t *a, input_event_t ev, uint32_t now) {
    flappy_t *f = &a->g.fl;
    if (ev != INPUT_BTN_A_PRESS) return ev == INPUT_BTN_A_SHORT || ev == INPUT_BTN_A_LONG;
    if (f->phase == FLAPPY_OVER) {
        if (!app_elapsed(now, f->t0, 800)) return true;  // don't restart by accident
        reset(a, now);
        f->phase = FLAPPY_START;
        return true;
    }
    if (f->phase == FLAPPY_START) {
        f->phase = FLAPPY_PLAY;
        f->last = now;
    }
    f->vy = FLAP_VY;
    app_play(a, SND_FLAP);
    return true;
}

static bool hits(const flappy_t *f, int i) {
    const int px = f->pipe[i].x / 16, by = f->y / 16;
    if (BIRD_X + BIRD_R - 1 < px || BIRD_X - BIRD_R + 1 > px + PIPE_W) return false;
    return by - BIRD_R + 1 < f->pipe[i].gap - PIPE_GAP / 2 || by + BIRD_R - 1 > f->pipe[i].gap + PIPE_GAP / 2;
}

static void crash(app_t *a, uint32_t now) {
    flappy_t *f = &a->g.fl;
    f->phase = FLAPPY_OVER;
    f->t0 = now;
    f->best = app_record(a, &a->save.flappy_pts, (uint16_t)f->score, false);
    a->mood = f->best ? EXPR_EXCITED : EXPR_DIZZY;
    app_play(a, f->best ? SND_RECORD : SND_FAIL);
    app_reward(a, 4 + (f->score > 12 ? 12 : f->score), 3, 2 + f->score);
}

static void tick(app_t *a, uint32_t now) {
    flappy_t *f = &a->g.fl;
    f->vy += GRAVITY;
    if (f->vy > MAX_VY) f->vy = MAX_VY;
    f->y += f->vy;
    if (f->y < BIRD_R * 16) {  // bump the ceiling
        f->y = BIRD_R * 16;
        f->vy = 0;
    }
    if (f->y / 16 + BIRD_R >= GFX_H) {
        crash(a, now);
        return;
    }
    for (int i = 0; i < FLAPPY_PIPES; i++) {
        f->pipe[i].x -= PIPE_SPEED;
        if (f->pipe[i].x / 16 + PIPE_W < 0) {
            // Off the left edge: go round again behind the rightmost pipe.
            int far = f->pipe[0].x;
            for (int k = 1; k < FLAPPY_PIPES; k++) {
                if (f->pipe[k].x > far) far = f->pipe[k].x;
            }
            place_pipe(a, i, far + PIPE_SPACING * 16);
        }
        if (!f->pipe[i].passed && f->pipe[i].x / 16 + PIPE_W < BIRD_X - BIRD_R) {
            f->pipe[i].passed = true;
            f->score++;
            app_play(a, SND_POINT);
        }
        if (hits(f, i)) {
            crash(a, now);
            return;
        }
    }
}

void flappy_update(app_t *a, uint32_t now) {
    flappy_t *f = &a->g.fl;
    if (f->phase != FLAPPY_PLAY) return;
    f->acc += now - f->last;
    f->last = now;
    if (f->acc > 10 * TICK_MS) f->acc = 10 * TICK_MS;  // a long stall: don't fast-forward
    while (f->acc >= TICK_MS && f->phase == FLAPPY_PLAY) {
        f->acc -= TICK_MS;
        tick(a, now);
    }
}

static void bird(gfx_t *g, int y, int vy, uint32_t now) {
    gfx_fill_circle(g, BIRD_X, y, BIRD_R, GFX_WHITE);
    gfx_pixel(g, BIRD_X + 1, y - 1, GFX_BLACK);
    gfx_pixel(g, BIRD_X + 3, y - 1, GFX_BLACK);
    // Wing: up right after a flap, down while falling; flutters on the title.
    const bool up = vy < 0 || (vy == 0 && (now / 200) % 2);
    if (up) {
        gfx_fill_triangle(g, BIRD_X - 2, y, BIRD_X - 9, y - 6, BIRD_X - 5, y + 1, GFX_WHITE);
    } else {
        gfx_fill_triangle(g, BIRD_X - 2, y, BIRD_X - 9, y + 5, BIRD_X - 5, y - 1, GFX_WHITE);
    }
}

static void pipe(gfx_t *g, int x, int gap) {
    const int top = gap - PIPE_GAP / 2, bot = gap + PIPE_GAP / 2;
    // Icicle from the top: a column ending in a point.
    gfx_fill_rect(g, x, 0, PIPE_W, top - 3, GFX_WHITE);
    gfx_fill_triangle(g, x, top - 4, x + PIPE_W - 1, top - 4, x + PIPE_W / 2, top, GFX_WHITE);
    // Snowdrift from the bottom: a column with a rounded cap.
    gfx_fill_rrect(g, x - 1, bot, PIPE_W + 2, GFX_H - bot + 4, 4, GFX_WHITE);
}

void flappy_render(const app_t *a, gfx_t *g) {
    const flappy_t *f = &a->g.fl;
    if (f->phase == FLAPPY_OVER) {
        app_game_over(g, f->score, f->best, a->save.flappy_pts);
        return;
    }
    for (int i = 0; i < FLAPPY_PIPES; i++) pipe(g, f->pipe[i].x / 16, f->pipe[i].gap);
    const int bob = f->phase == FLAPPY_START ? app_sin64((int)(a->now / 30)) * 3 / 256 : 0;
    bird(g, f->y / 16 + bob, f->vy, a->now);
    if (f->phase == FLAPPY_START) {
        font_draw_centered(g, 80, 26, "A: FLAP", 1, GFX_WHITE);
    } else {
        char n[8];
        app_itoa(f->score, n);
        // Score over a black box, so it reads over an icicle too.
        const int w = font_width(n, 2);
        gfx_fill_rect(g, 64 - w / 2 - 2, 1, w + 4, 17, GFX_BLACK);
        font_draw_centered(g, 64, 3, n, 2, GFX_WHITE);
    }
}

bool flappy_animating(const app_t *a) {
    return a->g.fl.phase != FLAPPY_OVER;
}
