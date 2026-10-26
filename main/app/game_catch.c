// Catch: tilt the keychain to slide the Yeti left and right, catch the
// snowflakes, dodge the rocks. Three lives; it speeds up as you score.
//
// Tilt maps to a position, not a speed: holding the keychain at an angle
// parks the Yeti there, which is far easier to control than a rolling ball.

#include "app/app_internal.h"

#define PLAYER_Y 54     // top of the player
#define PLAYER_HALF 9   // half width
#define TILT_FULL_MG 450  // this much tilt reaches the screen edge (~27 degrees)
#define FALL_START 24     // px per second at score 0
#define FALL_PER_POINT 2
#define FALL_MAX 80
#define SPAWN_START_MS 900
#define SPAWN_MIN_MS 380
#define ROCK_PCT 30

void catch_start(app_t *a, uint32_t now) {
    catch_t *c = &a->g.ctch;
    c->phase = CATCH_START;
    c->x = 64;
    c->t0 = c->last = now;
}

static void begin(app_t *a, uint32_t now) {
    catch_t *c = &a->g.ctch;
    for (int i = 0; i < CATCH_MAX; i++) c->obj[i].on = false;
    c->phase = CATCH_PLAY;
    c->lives = 3;
    c->score = 0;
    c->best = false;
    c->next_spawn = now + 600;
    c->last = c->t0 = now;
}

bool catch_event(app_t *a, input_event_t ev, uint32_t now) {
    catch_t *c = &a->g.ctch;
    if (ev != INPUT_BTN_A_PRESS) return ev == INPUT_BTN_A_SHORT || ev == INPUT_BTN_A_LONG;
    if (c->phase == CATCH_START || (c->phase == CATCH_OVER && app_elapsed(now, c->t0, 800))) {
        begin(a, now);
    }
    return true;
}

static void spawn(app_t *a) {
    catch_t *c = &a->g.ctch;
    for (int i = 0; i < CATCH_MAX; i++) {
        if (c->obj[i].on) continue;
        c->obj[i].on = true;
        c->obj[i].rock = rng_chance(&a->rng, ROCK_PCT);
        c->obj[i].x = (int)rng_range(&a->rng, 8, GFX_W - 9);
        c->obj[i].y = 0;
        return;
    }
}

void catch_update(app_t *a, uint32_t now) {
    catch_t *c = &a->g.ctch;
    int mg = 0;
    if (a->env && a->env->tilt && a->env->tilt(&mg)) {
        if (mg > TILT_FULL_MG) mg = TILT_FULL_MG;
        if (mg < -TILT_FULL_MG) mg = -TILT_FULL_MG;
        const int target = 64 + mg * (64 - PLAYER_HALF - 2) / TILT_FULL_MG;
        c->x += (target - c->x) / 2;  // smooth out sensor jitter
    }
    if (c->phase != CATCH_PLAY) return;

    const uint32_t dt = now - c->last;
    c->last = now;
    int speed = FALL_START + c->score * FALL_PER_POINT;
    if (speed > FALL_MAX) speed = FALL_MAX;
    for (int i = 0; i < CATCH_MAX; i++) {
        if (!c->obj[i].on) continue;
        c->obj[i].y += (int)(speed * 16 * dt / 1000);
        const int y = c->obj[i].y / 16;
        const int dx = c->obj[i].x - c->x;
        if (y >= PLAYER_Y - 3 && y <= PLAYER_Y + 4 && dx > -PLAYER_HALF - 3 && dx < PLAYER_HALF + 3) {
            c->obj[i].on = false;
            if (c->obj[i].rock) {
                c->lives--;
                app_play(a, SND_FAIL);
            } else {
                c->score++;
                app_play(a, SND_POINT);
            }
        } else if (y > GFX_H + 4) {
            c->obj[i].on = false;
        }
    }
    if (c->lives <= 0) {
        c->phase = CATCH_OVER;
        c->t0 = now;
        c->best = app_record(a, &a->save.catch_pts, (uint16_t)c->score, false);
        a->mood = c->best ? EXPR_EXCITED : EXPR_HAPPY;
        app_play(a, c->best ? SND_RECORD : SND_OVER);
        app_reward(a, 4 + (c->score > 16 ? 16 : c->score), 3, 2 + c->score / 2);
        return;
    }
    if (app_elapsed(now, c->next_spawn, 0)) {
        spawn(a);
        int gap = SPAWN_START_MS - c->score * 20;
        if (gap < SPAWN_MIN_MS) gap = SPAWN_MIN_MS;
        c->next_spawn = now + (uint32_t)gap;
    }
}

static void flake(gfx_t *g, int x, int y) {
    gfx_hline(g, x - 3, x + 3, y, GFX_WHITE);
    gfx_line(g, x - 2, y - 3, x + 2, y + 3, GFX_WHITE);
    gfx_line(g, x + 2, y - 3, x - 2, y + 3, GFX_WHITE);
}

static void player(gfx_t *g, int x) {
    // The Yeti peeking up from the bottom edge: a white dome with dot eyes.
    gfx_fill_rrect(g, x - PLAYER_HALF, PLAYER_Y, 2 * PLAYER_HALF + 1, 14, 6, GFX_WHITE);
    gfx_fill_circle(g, x - 4, PLAYER_Y + 4, 1, GFX_BLACK);
    gfx_fill_circle(g, x + 4, PLAYER_Y + 4, 1, GFX_BLACK);
    gfx_fill_rect(g, x - 2, PLAYER_Y + 7, 5, 1, GFX_BLACK);
}

static void score_text(const app_t *a, gfx_t *g, int score) {
    (void)a;
    char n[8];
    app_itoa(score, n);
    font_draw(g, GFX_W - 2 - font_width(n, 1), 2, n, 1, GFX_WHITE);
}

void catch_render(const app_t *a, gfx_t *g) {
    const catch_t *c = &a->g.ctch;
    if (c->phase == CATCH_START) {
        font_draw_centered(g, 64, 6, "TILT TO MOVE", 1, GFX_WHITE);
        flake(g, 40, 24);
        gfx_fill_circle(g, 88, 26, 3, GFX_WHITE);
        font_draw(g, 30, 32, "+1", 1, GFX_WHITE);
        font_draw(g, 80, 32, "-1", 1, GFX_WHITE);
        font_draw_centered(g, 64, 44, "A:START  B:BACK", 1, GFX_WHITE);
        player(g, c->x);
        return;
    }
    if (c->phase == CATCH_OVER) {
        app_game_over(g, c->score, c->best, a->save.catch_pts);
        return;
    }
    for (int i = 0; i < c->lives; i++) app_heart(g, 5 + i * 9, 5, 7);
    score_text(a, g, c->score);
    for (int i = 0; i < CATCH_MAX; i++) {
        if (!c->obj[i].on) continue;
        const int y = c->obj[i].y / 16;
        if (c->obj[i].rock) {
            gfx_fill_circle(g, c->obj[i].x, y, 3, GFX_WHITE);
        } else {
            flake(g, c->obj[i].x, y);
        }
    }
    player(g, c->x);
}

bool catch_animating(const app_t *a) {
    return a->g.ctch.phase != CATCH_OVER;  // the player follows the tilt even before start
}
