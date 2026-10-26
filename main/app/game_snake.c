// Snake with two buttons: A turns left, B turns right (relative to where the
// snake is heading). Eat the berries, don't hit the wall or yourself. It
// speeds up as it grows.
//
// Turns use the PRESS events, so a turn happens the moment the button goes
// down; up to two are queued, so a quick A-A U-turn works.

#include "app/app_internal.h"

#define CELL 4
#define FIELD_X 4  // top-left of cell (0, 0)
#define FIELD_Y 4
#define STEP_START_MS 190
#define STEP_MIN_MS 80
#define STEP_PER_FOOD 5
#define START_LEN 4

static const int DX[4] = {1, 0, -1, 0};
static const int DY[4] = {0, 1, 0, -1};

// i-th segment from the head (0 = head).
static const uint8_t *seg(const snake_t *s, int i) {
    return s->body[(s->head - i + SNAKE_MAX) % SNAKE_MAX];
}

static bool occupied(const snake_t *s, int x, int y, int skip_tail) {
    for (int i = 0; i < s->len - skip_tail; i++) {
        const uint8_t *c = seg(s, i);
        if (c[0] == x && c[1] == y) return true;
    }
    return false;
}

static void place_food(app_t *a) {
    snake_t *s = &a->g.snake;
    if (s->len >= SNAKE_MAX) return;
    int x, y;
    do {
        x = (int)rng_range(&a->rng, 0, SNAKE_COLS - 1);
        y = (int)rng_range(&a->rng, 0, SNAKE_ROWS - 1);
    } while (occupied(s, x, y, 0));
    s->food[0] = x;
    s->food[1] = y;
}

static void begin(app_t *a, uint32_t now) {
    snake_t *s = &a->g.snake;
    s->len = START_LEN;
    s->head = START_LEN - 1;
    for (int i = 0; i < START_LEN; i++) {
        s->body[i][0] = (uint8_t)(6 + i);
        s->body[i][1] = SNAKE_ROWS / 2;
    }
    s->dir = 0;
    s->nturns = 0;
    s->score = 0;
    s->best = false;
    s->phase = SNAKE_PLAY;
    s->next_step = now + STEP_START_MS;
    place_food(a);
}

void snake_start(app_t *a, uint32_t now) {
    a->g.snake.phase = SNAKE_START;
    a->g.snake.t0 = now;
}

bool snake_event(app_t *a, input_event_t ev, uint32_t now) {
    snake_t *s = &a->g.snake;
    if (ev == INPUT_BTN_A_SHORT || ev == INPUT_BTN_A_LONG) return true;
    if (ev == INPUT_BTN_B_SHORT && s->swallow_b) {
        s->swallow_b = false;
        return true;
    }
    if (s->phase != SNAKE_PLAY) {
        if (ev != INPUT_BTN_A_PRESS) return false;  // B goes back to the menu
        if (s->phase == SNAKE_START || app_elapsed(now, s->t0, 800)) begin(a, now);
        return true;
    }
    if (ev != INPUT_BTN_A_PRESS && ev != INPUT_BTN_B_PRESS) return false;
    if (ev == INPUT_BTN_B_PRESS) s->swallow_b = true;
    if (s->nturns < 2) s->turns[s->nturns++] = ev == INPUT_BTN_A_PRESS ? -1 : 1;
    return true;
}

static void die(app_t *a, uint32_t now) {
    snake_t *s = &a->g.snake;
    s->phase = SNAKE_OVER;
    s->t0 = now;
    s->best = app_record(a, &a->save.snake_pts, (uint16_t)s->score, false);
    a->mood = s->best ? EXPR_EXCITED : EXPR_KO;
    app_play(a, s->best ? SND_RECORD : SND_FAIL);
    app_reward(a, 4 + (s->score > 16 ? 16 : s->score), 3, 2 + s->score);
}

static void step(app_t *a, uint32_t now) {
    snake_t *s = &a->g.snake;
    if (s->nturns) {
        s->dir = (s->dir + s->turns[0] + 4) % 4;
        s->turns[0] = s->turns[1];
        s->nturns--;
    }
    const uint8_t *h = seg(s, 0);
    const int x = h[0] + DX[s->dir], y = h[1] + DY[s->dir];
    const bool eat = x == s->food[0] && y == s->food[1];
    // The tail moves out of the way this step unless we grow.
    if (x < 0 || y < 0 || x >= SNAKE_COLS || y >= SNAKE_ROWS || occupied(s, x, y, eat ? 0 : 1)) {
        die(a, now);
        return;
    }
    s->head = (s->head + 1) % SNAKE_MAX;
    s->body[s->head][0] = (uint8_t)x;
    s->body[s->head][1] = (uint8_t)y;
    if (eat) {
        s->len++;
        s->score++;
        app_play(a, SND_POINT);
        place_food(a);
    }
}

void snake_update(app_t *a, uint32_t now) {
    snake_t *s = &a->g.snake;
    // A late frame does several steps, but never more than a few.
    for (int k = 0; k < 4 && s->phase == SNAKE_PLAY && app_elapsed(now, s->next_step, 0); k++) {
        step(a, now);
        int ms = STEP_START_MS - s->score * STEP_PER_FOOD;
        if (ms < STEP_MIN_MS) ms = STEP_MIN_MS;
        s->next_step += (uint32_t)ms;
    }
    if (s->phase == SNAKE_PLAY && app_elapsed(now, s->next_step + 500, 0)) s->next_step = now;
}

void snake_render(const app_t *a, gfx_t *g) {
    const snake_t *s = &a->g.snake;
    if (s->phase == SNAKE_START) {
        font_draw_centered(g, 64, 4, "SNAKE", 2, GFX_WHITE);
        font_draw_centered(g, 64, 24, "A: LEFT  B: RIGHT", 1, GFX_WHITE);
        for (int i = 0; i < 6; i++) gfx_fill_rect(g, 40 + i * 6, 38, 5, 5, GFX_WHITE);
        gfx_fill_circle(g, 88, 40, 2, GFX_WHITE);
        app_hint(g, "A:START", "B:BACK");
        return;
    }
    if (s->phase == SNAKE_OVER) {
        app_game_over(g, s->score, s->best, a->save.snake_pts);
        return;
    }
    gfx_rect(g, FIELD_X - 2, FIELD_Y - 2, SNAKE_COLS * CELL + 3, SNAKE_ROWS * CELL + 3, GFX_WHITE);
    for (int i = 0; i < s->len; i++) {
        const uint8_t *c = seg(s, i);
        gfx_fill_rect(g, FIELD_X + c[0] * CELL, FIELD_Y + c[1] * CELL, CELL - 1, CELL - 1, GFX_WHITE);
    }
    // Berry: a plus, blinking slowly so it stands out from the body.
    const int fx = FIELD_X + s->food[0] * CELL + 1, fy = FIELD_Y + s->food[1] * CELL + 1;
    if ((a->now / 250) % 4) {
        gfx_fill_rect(g, fx - 1, fy, 3, 1, GFX_WHITE);
        gfx_fill_rect(g, fx, fy - 1, 1, 3, GFX_WHITE);
    }
}

bool snake_animating(const app_t *a) {
    return a->g.snake.phase == SNAKE_PLAY;
}
