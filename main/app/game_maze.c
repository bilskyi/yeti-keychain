// Maze: hold the keychain flat like a tray and tilt it to roll the ball to
// the ring. Every maze solved adds time and the next one is bigger; the run
// ends when the time bar at the top runs out. One point per maze.
//
// The ball rolls downhill from wherever the keychain was held at the start,
// so it doesn't have to be perfectly level. Positions are in 1/16 px,
// speeds in 1/16 px per second.

#include "app/app_internal.h"

#define RIGHT 1  // walls[][] bits: the cell's right and bottom walls
#define DOWN 2
#define AREA_W 120
#define AREA_H 56
#define AREA_Y 5        // below the time bar
#define BALL_R 2
#define START_MS 45000
#define MAX_MS 99000
#define BONUS_MS 8000   // per maze, plus BONUS_PER_CELL for each cell in it
#define BONUS_PER_CELL 120
#define SOLVED_MS 700   // "NICE!" before the next maze
#define DEAD_MG 40      // tilts smaller than this don't move the ball
#define ACCEL 64        // 1/16 px/s per mg per second / 10: 1 g is ~400 px/s/s
#define FRICTION 2      // of the speed lost per second
#define SPEED_MAX (100 * 16)

// Cell sizes, level by level: big rooms first, then ever more corridors.
static const uint8_t CELL[] = {16, 12, 10, 8};

static int maze_cells(const maze_t *m) {
    return m->cols * m->rows;
}

static void generate(app_t *a) {
    maze_t *m = &a->g.maze;
    const int lv = m->level < (int)sizeof(CELL) ? m->level : (int)sizeof(CELL) - 1;
    m->cell = CELL[lv];
    m->cols = AREA_W / m->cell;
    m->rows = AREA_H / m->cell;
    m->ox = (GFX_W - m->cols * m->cell) / 2;
    m->oy = AREA_Y + (AREA_H - m->rows * m->cell) / 2;
    for (int r = 0; r < m->rows; r++) {
        for (int c = 0; c < m->cols; c++) m->walls[r][c] = RIGHT | DOWN;
    }

    // Recursive backtracker, with an explicit stack: every cell is reachable
    // by exactly one path. The goal is where the stack was deepest, the cell
    // farthest along its path from the start.
    static const int DC[4] = {-1, 1, 0, 0}, DR[4] = {0, 0, -1, 1};
    uint8_t stack[MAZE_ROWS * MAZE_COLS][2];  // (col, row)
    bool seen[MAZE_ROWS][MAZE_COLS] = {{false}};
    int sp = 0, deepest = 0;
    stack[sp][0] = stack[sp][1] = 0;
    sp++;
    seen[0][0] = true;
    m->goal = 0;
    while (sp > 0) {
        const int c = stack[sp - 1][0], r = stack[sp - 1][1];
        int dirs[4], n = 0;
        for (int d = 0; d < 4; d++) {
            const int nc = c + DC[d], nr = r + DR[d];
            if (nc >= 0 && nc < m->cols && nr >= 0 && nr < m->rows && !seen[nr][nc]) dirs[n++] = d;
        }
        if (n == 0) {
            sp--;
            continue;
        }
        const int d = dirs[rng_range(&a->rng, 0, (uint32_t)n - 1)];
        const int nc = c + DC[d], nr = r + DR[d];
        if (d == 0) m->walls[r][nc] &= (uint8_t)~RIGHT;
        if (d == 1) m->walls[r][c] &= (uint8_t)~RIGHT;
        if (d == 2) m->walls[nr][c] &= (uint8_t)~DOWN;
        if (d == 3) m->walls[r][c] &= (uint8_t)~DOWN;
        seen[nr][nc] = true;
        stack[sp][0] = (uint8_t)nc;
        stack[sp][1] = (uint8_t)nr;
        sp++;
        if (sp > deepest) {
            deepest = sp;
            m->goal = nr * m->cols + nc;
        }
    }

    m->x = (m->ox + m->cell / 2) * 16;
    m->y = (m->oy + m->cell / 2) * 16;
    m->vx = m->vy = 0;
}

// The wall between columns k-1 and k in row r (k = 0 and cols are the sides),
// and the one between rows j-1 and j in column c.
static bool vwall(const maze_t *m, int k, int r) {
    return k <= 0 || k >= m->cols || (m->walls[r][k - 1] & RIGHT);
}
static bool hwall(const maze_t *m, int c, int j) {
    return j <= 0 || j >= m->rows || (m->walls[j - 1][c] & DOWN);
}

// Whether screen pixel (x, y) is wall. A corner post is drawn only where a
// wall meets it, so open floor has no stray dots.
static bool maze_wall_at(const maze_t *m, int x, int y) {
    const int rx = x - m->ox, ry = y - m->oy;
    if (rx < 0 || ry < 0 || rx > m->cols * m->cell || ry > m->rows * m->cell) return true;
    const int k = rx / m->cell, j = ry / m->cell;
    const bool on_v = rx % m->cell == 0, on_h = ry % m->cell == 0;
    if (on_v && on_h) {
        return (j > 0 && vwall(m, k, j - 1)) || (j < m->rows && vwall(m, k, j)) ||
               (k > 0 && hwall(m, k - 1, j)) || (k < m->cols && hwall(m, k, j));
    }
    if (on_v) return vwall(m, k, j);
    if (on_h) return hwall(m, k, j);
    return false;
}

static bool blocked(const maze_t *m, int x16, int y16) {
    const int x = x16 / 16, y = y16 / 16;
    for (int dy = -BALL_R; dy <= BALL_R; dy++) {
        for (int dx = -BALL_R; dx <= BALL_R; dx++) {
            if (maze_wall_at(m, x + dx, y + dy)) return true;
        }
    }
    return false;
}

// Moves one axis a pixel at a time, so a fast ball can't pass through a
// wall; hitting one takes most of the speed out and bounces the rest.
static void roll(const maze_t *m, int *pos, int *v, int d, bool horizontal) {
    while (d != 0) {
        const int step = d > 16 ? 16 : d < -16 ? -16 : d;
        const int nx = horizontal ? *pos + step : m->x, ny = horizontal ? m->y : *pos + step;
        if (blocked(m, nx, ny)) {
            *v = -*v / 4;
            return;
        }
        *pos += step;
        d -= step;
    }
}

static int goal_x(const maze_t *m) {
    return m->ox + (m->goal % m->cols) * m->cell + m->cell / 2;
}
static int goal_y(const maze_t *m) {
    return m->oy + (m->goal / m->cols) * m->cell + m->cell / 2;
}

static bool tilt(const app_t *a, int mg[2]) {
    return a->env && a->env->tilt_xy && a->env->tilt_xy(mg);
}

void maze_start(app_t *a, uint32_t now) {
    maze_t *m = &a->g.maze;
    m->phase = MAZE_START;
    m->level = 0;
    m->t0 = m->last = now;
}

static void begin(app_t *a, uint32_t now) {
    maze_t *m = &a->g.maze;
    m->phase = MAZE_PLAY;
    m->level = 0;
    m->score = 0;
    m->best = false;
    m->left_ms = START_MS;
    m->base[0] = m->base[1] = 0;
    tilt(a, m->base);  // "level" is however it is held now
    generate(a);
    m->t0 = m->last = now;
}

bool maze_event(app_t *a, input_event_t ev, uint32_t now) {
    maze_t *m = &a->g.maze;
    if (ev != INPUT_BTN_A_PRESS) return ev == INPUT_BTN_A_SHORT || ev == INPUT_BTN_A_LONG;
    if (m->phase == MAZE_START || (m->phase == MAZE_OVER && app_elapsed(now, m->t0, 800))) {
        begin(a, now);
    }
    return true;
}

static int axis_mg(int mg, int base) {
    int d = mg - base;
    if (d > -DEAD_MG && d < DEAD_MG) return 0;
    d += d > 0 ? -DEAD_MG : DEAD_MG;
    return d > 1000 ? 1000 : d < -1000 ? -1000 : d;
}

static int clamp_speed(int v) {
    return v > SPEED_MAX ? SPEED_MAX : v < -SPEED_MAX ? -SPEED_MAX : v;
}

void maze_update(app_t *a, uint32_t now) {
    maze_t *m = &a->g.maze;
    const int dt = (int)(now - m->last) > 100 ? 100 : (int)(now - m->last);  // after a stall, don't leap
    m->last = now;
    if (m->phase == MAZE_SOLVED) {
        if (app_elapsed(now, m->t0, SOLVED_MS)) {
            m->level++;
            generate(a);
            m->phase = MAZE_PLAY;
        }
        return;
    }
    if (m->phase != MAZE_PLAY) return;

    m->left_ms -= dt;
    if (m->left_ms <= 0) {
        m->left_ms = 0;
        m->phase = MAZE_OVER;
        m->t0 = now;
        m->best = app_record(a, &a->save.maze_pts, (uint16_t)m->score, false);
        a->mood = m->best ? EXPR_EXCITED : EXPR_HAPPY;
        app_play(a, m->best ? SND_RECORD : SND_OVER);
        return;
    }

    int mg[2];
    if (tilt(a, mg)) {
        m->vx = clamp_speed(m->vx + axis_mg(mg[0], m->base[0]) * dt * ACCEL / 10000);
        m->vy = clamp_speed(m->vy + axis_mg(mg[1], m->base[1]) * dt * ACCEL / 10000);
    }
    m->vx -= m->vx * FRICTION * dt / 1000;
    m->vy -= m->vy * FRICTION * dt / 1000;
    roll(m, &m->x, &m->vx, m->vx * dt / 1000, true);
    roll(m, &m->y, &m->vy, m->vy * dt / 1000, false);

    const int dx = m->x / 16 - goal_x(m), dy = m->y / 16 - goal_y(m);
    if (dx >= -2 && dx <= 2 && dy >= -2 && dy <= 2) {
        m->score++;
        m->left_ms += BONUS_MS + BONUS_PER_CELL * maze_cells(m);
        if (m->left_ms > MAX_MS) m->left_ms = MAX_MS;
        m->phase = MAZE_SOLVED;
        m->t0 = now;
        app_play(a, SND_POINT);
    }
}

static void draw_maze(const maze_t *m, gfx_t *g) {
    for (int y = m->oy; y <= m->oy + m->rows * m->cell; y++) {
        for (int x = m->ox; x <= m->ox + m->cols * m->cell; x++) {
            if (maze_wall_at(m, x, y)) gfx_pixel(g, x, y, GFX_WHITE);
        }
    }
}

static void ring(gfx_t *g, int cx, int cy) {
    gfx_fill_circle(g, cx, cy, 3, GFX_WHITE);
    gfx_fill_circle(g, cx, cy, 1, GFX_BLACK);
}

void maze_render(const app_t *a, gfx_t *g) {
    const maze_t *m = &a->g.maze;
    if (m->phase == MAZE_START) {
        font_draw_centered(g, 64, 6, "HOLD IT FLAT", 1, GFX_WHITE);
        font_draw_centered(g, 64, 18, "TILT TO ROLL", 1, GFX_WHITE);
        gfx_fill_circle(g, 44, 38, BALL_R, GFX_WHITE);
        gfx_line(g, 50, 38, 72, 38, GFX_WHITE);
        gfx_line(g, 68, 35, 72, 38, GFX_WHITE);
        gfx_line(g, 68, 41, 72, 38, GFX_WHITE);
        ring(g, 82, 38);
        app_hint_start(g);
        return;
    }
    if (m->phase == MAZE_OVER) {
        app_game_over(g, m->score, m->best, a->save.maze_pts);
        return;
    }
    gfx_fill_rect(g, 0, 0, GFX_W * m->left_ms / MAX_MS + 1, 2, GFX_WHITE);
    draw_maze(m, g);
    ring(g, goal_x(m), goal_y(m));
    gfx_fill_circle(g, m->x / 16, m->y / 16, BALL_R, GFX_WHITE);
    if (m->phase == MAZE_SOLVED) {
        gfx_fill_rrect(g, 34, 20, 60, 24, 5, GFX_BLACK);
        gfx_fill_rrect(g, 36, 22, 56, 20, 4, GFX_WHITE);
        font_draw_centered(g, 64, 25, "NICE!", 2, GFX_BLACK);
    }
}

bool maze_animating(const app_t *a) {
    return a->g.maze.phase == MAZE_PLAY || a->g.maze.phase == MAZE_SOLVED;
}
