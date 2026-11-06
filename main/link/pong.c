// The same file in both projects; see pong.h.

#include "pong.h"

#include <string.h>

#define Q 256
#define R (PONG_BALL_R * Q)
#define REST_Y ((PONG_PADDLE_Y - PONG_BALL_R - 1) * Q)  // the ball sitting on the paddle
#define HALF_PADDLE (PONG_PADDLE_W / 2 * Q)

static uint32_t next_rand(pong_t *p) {
    p->rng = p->rng * 1664525u + 1013904223u;
    return p->rng >> 8;
}

static int32_t isqrt64(int64_t v) {
    if (v <= 0) return 0;
    int64_t x = v, y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + v / x) / 2;
    }
    return (int32_t)x;
}

static int32_t clamp32(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

// Sets the direction from a sideways share of the speed, -256..256 (Q8), the
// rest of it going up the screen.
static void aim_ball(pong_t *p, int32_t share) {
    share = clamp32(share, -200, 200);  // never so flat it crawls along
    p->vx = (int32_t)((int64_t)p->speed * share / Q);
    p->vy = -isqrt64((int64_t)p->speed * p->speed - (int64_t)p->vx * p->vx);
}

static void begin(pong_t *p, pong_phase_t phase, uint32_t now_ms) {
    p->phase = (uint8_t)phase;
    p->phase_at = now_ms;
}

static void rest_on_paddle(pong_t *p) {
    p->x = p->paddle;
    p->y = REST_Y;
    p->vx = p->vy = 0;
}

void pong_start(pong_t *p, bool serve, uint32_t seed, uint32_t now_ms) {
    memset(p, 0, sizeof(*p));
    p->rng = seed ? seed : 0x5EED;
    p->paddle = p->target = PONG_W / 2 * Q;
    p->speed = PONG_SPEED0;
    p->last_ms = p->heard_at = now_ms;
    begin(p, serve ? PONG_SERVE : PONG_AWAY, now_ms);
    if (serve) rest_on_paddle(p);
    p->away_x = PONG_W / 2 * Q;
}

void pong_aim(pong_t *p, int x_px) {
    p->target = clamp32(x_px * Q, HALF_PADDLE, PONG_W * Q - HALF_PADDLE);
}

void pong_launch(pong_t *p, uint32_t now_ms) {
    if (p->phase != PONG_SERVE) return;
    p->speed = PONG_SPEED0;
    aim_ball(p, (int32_t)(next_rand(p) % 241) - 120);
    begin(p, PONG_MINE, now_ms);
}

static void say(pong_t *p, const pong_out_t *o) {
    p->out = *o;
    p->out_ready = true;
}

bool pong_take_out(pong_t *p, pong_out_t *o) {
    if (!p->out_ready) return false;
    *o = p->out;
    p->out_ready = false;
    return true;
}

static void end(pong_t *p, pong_end_t why, uint32_t now_ms) {
    p->end = (uint8_t)why;
    begin(p, PONG_OVER, now_ms);
    p->events |= PONG_EV_OVER;
}

static void move_paddle(pong_t *p, uint32_t dt) {
    const int32_t step = (int32_t)((int64_t)PONG_PADDLE_SPEED * dt / 1000);
    const int32_t d = p->target - p->paddle;
    p->paddle += clamp32(d, -step, step);
}

static void i_missed(pong_t *p, uint32_t now_ms) {
    p->them++;
    p->events |= PONG_EV_MISSED;
    const pong_out_t o = {.kind = PONG_OUT_MISS, .yours = p->them, .mine = p->me};
    say(p, &o);
    if (p->them >= PONG_WIN) {
        end(p, PONG_END_LOST, now_ms);
        return;
    }
    // Whoever missed serves next.
    p->speed = PONG_SPEED0;
    begin(p, PONG_SERVE, now_ms);
    rest_on_paddle(p);
}

static void fly(pong_t *p, uint32_t dt, uint32_t now_ms) {
    const int32_t was_bottom = p->y + R;
    p->x += (int32_t)((int64_t)p->vx * dt / 1000);
    p->y += (int32_t)((int64_t)p->vy * dt / 1000);

    if (p->x < R) {
        p->x = 2 * R - p->x;
        p->vx = -p->vx;
        p->events |= PONG_EV_WALL;
    } else if (p->x > (PONG_W - 1) * Q - R) {
        p->x = 2 * ((PONG_W - 1) * Q - R) - p->x;
        p->vx = -p->vx;
        p->events |= PONG_EV_WALL;
    }

    // The paddle: only on the way down, only the frame the ball reaches it.
    const int32_t top = PONG_PADDLE_Y * Q;
    if (p->vy > 0 && was_bottom < top && p->y + R >= top) {
        const int32_t off = p->x - p->paddle;
        if (off >= -(HALF_PADDLE + R) && off <= HALF_PADDLE + R) {
            p->y = top - R - 1;
            p->speed = p->speed * (100 + PONG_SPEEDUP) / 100;
            if (p->speed > PONG_SPEED_MAX) p->speed = PONG_SPEED_MAX;
            // Where it struck sends it: the middle straight back, the ends
            // off at an angle.
            aim_ball(p, (int32_t)((int64_t)off * 200 / (HALF_PADDLE + R)));
            p->events |= PONG_EV_HIT;
            return;
        }
    }

    // Gone over the top: it is theirs now.
    if (p->vy < 0 && p->y < -R) {
        p->away_x = p->x;
        const pong_out_t o = {.kind = PONG_OUT_BALL, .x = p->x, .vx = p->vx, .vy = p->vy};
        say(p, &o);
        begin(p, PONG_AWAY, now_ms);
        return;
    }
    // Gone past the paddle.
    if (p->y - R > PONG_H * Q) i_missed(p, now_ms);
}

// Events gather in p->events - from the other side's news between ticks as
// well as from the tick itself - and the tick hands them over and clears them.
static uint8_t hand_over(pong_t *p) {
    const uint8_t ev = p->events;
    p->events = 0;
    return ev;
}

uint8_t pong_tick(pong_t *p, uint32_t now_ms) {
    uint32_t dt = now_ms - p->last_ms;
    p->last_ms = now_ms;
    if (dt > PONG_STEP_MAX_MS) dt = PONG_STEP_MAX_MS;
    if (p->phase == PONG_OVER) return hand_over(p);

    // Signed: "heard" can be stamped by the radio's task a moment after this
    // tick read its clock, and an unsigned "now - then" of -1 ms is 49 days of
    // silence. That ended matches at random, mid-rally, on the bench.
    if ((int32_t)(now_ms - p->heard_at) >= (int32_t)PONG_LOST_MS) {
        end(p, PONG_END_GONE, now_ms);
        return hand_over(p);
    }

    move_paddle(p, dt);
    switch (p->phase) {
        case PONG_SERVE:
            rest_on_paddle(p);
            if ((uint32_t)(now_ms - p->phase_at) >= PONG_SERVE_MS) pong_launch(p, now_ms);
            break;
        case PONG_MINE:
            fly(p, dt, now_ms);
            break;
        default:
            break;
    }
    return hand_over(p);
}

void pong_ball_in(pong_t *p, int32_t x, int32_t vx, int32_t vy, uint32_t now_ms) {
    pong_heard(p, now_ms);
    if (p->phase != PONG_AWAY) return;
    // Facing each other: their left is my right, their up is my down.
    p->x = clamp32((PONG_W - 1) * Q - x, R, (PONG_W - 1) * Q - R);
    p->y = -R;
    p->vx = -vx;
    p->vy = vy < 0 ? -vy : vy;
    if (p->vy < PONG_SPEED0 / 4) p->vy = PONG_SPEED0 / 4;  // a ball that would never come down
    p->speed = isqrt64((int64_t)p->vx * p->vx + (int64_t)p->vy * p->vy);
    begin(p, PONG_MINE, now_ms);
    p->events |= PONG_EV_ARRIVED;
}

void pong_they_missed(pong_t *p, uint8_t mine, uint8_t theirs, uint32_t now_ms) {
    pong_heard(p, now_ms);
    if (p->phase == PONG_OVER) return;
    p->me = mine;
    p->them = theirs;
    p->events |= PONG_EV_SCORED;
    if (p->me >= PONG_WIN) {
        end(p, PONG_END_WON, now_ms);
        return;
    }
    // They serve. A ball still shown on my side is theirs to serve now.
    begin(p, PONG_AWAY, now_ms);
    p->away_x = PONG_W / 2 * Q;
}

void pong_they_quit(pong_t *p, uint32_t now_ms) {
    pong_heard(p, now_ms);
    if (p->phase != PONG_OVER) end(p, PONG_END_THEY_QUIT, now_ms);
}

void pong_heard(pong_t *p, uint32_t now_ms) { p->heard_at = now_ms; }
