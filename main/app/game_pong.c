// PONG: a match against BIT across both screens (link/pong.h has the rules).
// A asks BIT; BIT has a few seconds to say yes on its own screen. YETI serves
// first. Tilt the keychain to move the paddle, A serves at once, C leaves.
// First to PONG_WIN.

#include "app/app_internal.h"
#include "app/art.h"

#define ASK_MS 15000      // BIT's card waits 12 s; a little longer for its answer to arrive
#define NO_MS 2600
#define ALIVE_MS 500
#define TILT_FULL_MG 450  // as Catch: this much tilt reaches the edge
#define FIRST_HINT_MS 3500

static void begin(pongk_t *p, int phase, uint32_t now) {
    p->phase = phase;
    p->t0 = now;
}

void pongk_start(app_t *a, uint32_t now) {
    begin(&a->g.pong, PK_TITLE, now);
}

static void say(app_t *a, uint8_t type, bool reliable) {
    const bl_msg_t m = {.type = type};
    (void)app_link_send(a, &m, reliable);
}

static void ask(app_t *a, uint32_t now) {
    pongk_t *p = &a->g.pong;
    const app_link_t st = app_link_status(a);
    if (st == APP_LINK_OFF || st == APP_LINK_SEARCHING) {
        p->why = TOSS_NO_BIT;
        begin(p, PK_NO, now);
        app_play(a, SND_FAIL);
        return;
    }
    say(a, BL_INVITE, true);
    begin(p, PK_ASKING, now);
    app_play(a, SND_SELECT);
}

static void over(app_t *a, uint8_t end, uint32_t now) {
    pongk_t *p = &a->g.pong;
    APP_LOG("pong over: end %u, %u:%u, BIT last heard %u ms before", end, p->pong.me, p->pong.them,
            (unsigned)(now - p->pong.heard_at));
    p->end = end;
    begin(p, PK_OVER, now);
    if (end == PONG_END_WON) {
        app_play(a, SND_RECORD);
        a->mood = EXPR_EXCITED;
    } else {
        app_play(a, SND_OVER);
        if (end == PONG_END_LOST) a->mood = EXPR_SAD;
    }
}

void pongk_leave(app_t *a) {
    pongk_t *p = &a->g.pong;
    if (p->phase == PK_PLAY || p->phase == PK_ASKING) say(a, BL_QUIT, true);
    p->phase = PK_TITLE;
}

bool pongk_event(app_t *a, input_event_t ev, uint32_t now) {
    pongk_t *p = &a->g.pong;
    switch (p->phase) {
        case PK_TITLE:
        case PK_OVER:
            if (ev == INPUT_BTN_A_SHORT) {
                ask(a, now);
                return true;
            }
            return false;
        case PK_ASKING:
            if (ev == INPUT_BTN_C_SHORT) {  // never mind
                pongk_leave(a);
                app_play(a, SND_BACK);
                return true;
            }
            return false;
        case PK_PLAY:
            if (ev == INPUT_BTN_A_SHORT || ev == INPUT_BTN_A_PRESS) {
                pong_launch(&p->pong, now);
                return true;
            }
            if (ev == INPUT_BTN_C_SHORT) {
                say(a, BL_QUIT, true);
                over(a, PK_I_QUIT, now);
                return true;
            }
            return false;
        default:
            return false;
    }
}

void pongk_link(app_t *a, const bl_msg_t *m, uint32_t now) {
    pongk_t *p = &a->g.pong;
    if (p->phase == PK_ASKING && m->type == BL_ANSWER) {
        if (m->d) {
            pong_start(&p->pong, true, rng_next(&a->rng), now);
            p->point_at = 0;
            p->next_alive = now;
            begin(p, PK_PLAY, now);
            app_play(a, SND_GO);
        } else {
            p->why = m->e;
            begin(p, PK_NO, now);
            app_play(a, SND_FAIL);
        }
        return;
    }
    if (p->phase != PK_PLAY) return;
    pong_heard(&p->pong, now);
    switch (m->type) {
        case BL_BALL: pong_ball_in(&p->pong, m->a, m->b, m->c, now); break;
        case BL_MISS: pong_they_missed(&p->pong, m->d, m->e, now); break;
        case BL_QUIT: pong_they_quit(&p->pong, now); break;
        default: break;
    }
}

void pongk_link_done(app_t *a, const bl_msg_t *m, bool delivered, uint32_t now) {
    pongk_t *p = &a->g.pong;
    // The question never reached BIT.
    if (p->phase == PK_ASKING && m->type == BL_INVITE && !delivered) {
        p->why = 0;
        begin(p, PK_NO, now);
        app_play(a, SND_FAIL);
    }
}

static void play(app_t *a, uint32_t now) {
    pongk_t *p = &a->g.pong;
    int mg;
    if (a->env && a->env->tilt && a->env->tilt(&mg)) pong_aim(&p->pong, 64 + mg * 58 / TILT_FULL_MG);
    if (a->env && a->env->link_heard_at) {
        const uint32_t h = a->env->link_heard_at();
        if (h != 0 && (int32_t)(h - p->pong.heard_at) > 0) pong_heard(&p->pong, h);
    }
    const uint8_t ev = pong_tick(&p->pong, now);
    if (ev & PONG_EV_HIT) app_play(a, SND_FLAP);
    if (ev & PONG_EV_WALL) app_play(a, SND_MOVE);
    if (ev & PONG_EV_MISSED) {
        app_play(a, SND_FAIL);
        p->point_at = now;
        p->point_mine = false;
    }
    if (ev & PONG_EV_SCORED) {
        app_play(a, SND_POINT);
        p->point_at = now;
        p->point_mine = true;
    }
    pong_out_t o;
    while (pong_take_out(&p->pong, &o)) {
        bl_msg_t m = {0};
        if (o.kind == PONG_OUT_BALL) {
            m.type = BL_BALL;
            m.a = o.x;
            m.b = o.vx;
            m.c = o.vy;
        } else {
            m.type = BL_MISS;
            m.d = o.yours;
            m.e = o.mine;
        }
        (void)app_link_send(a, &m, true);
    }
    if (app_elapsed(now, p->next_alive, 0)) {
        p->next_alive = now + ALIVE_MS;
        say(a, BL_ALIVE, false);
    }
    if (pong_over(&p->pong)) over(a, p->pong.end, now);
}

void pongk_update(app_t *a, uint32_t now) {
    pongk_t *p = &a->g.pong;
    switch (p->phase) {
        case PK_ASKING:
            if (app_elapsed(now, p->t0, ASK_MS)) {
                say(a, BL_QUIT, true);
                p->why = BL_NO_TIMEOUT;
                begin(p, PK_NO, now);
            }
            break;
        case PK_NO:
            if (app_elapsed(now, p->t0, NO_MS)) begin(p, PK_TITLE, now);
            break;
        case PK_PLAY:
            play(a, now);
            break;
        default:
            break;
    }
}

static void digits(char *out, int v) { app_itoa(v, out); }

static void score_line(char *out, int a, int b) {
    char x[8], y[8];
    digits(x, a);
    digits(y, b);
    int n = 0;
    for (const char *c = x; *c; c++) out[n++] = *c;
    out[n++] = '-';
    for (const char *c = y; *c; c++) out[n++] = *c;
    out[n] = 0;
}

static void render_match(const app_t *a, gfx_t *g) {
    const pongk_t *pk = &a->g.pong;
    const pong_t *p = &pk->pong;
    // The net: the top edge, where the ball goes over to BIT.
    for (int x = 0; x < GFX_W; x += 4) gfx_hline(g, x, x + 1, 0, GFX_WHITE);
    char s[8];
    digits(s, p->me);
    font_draw(g, 2, 3, s, 1, GFX_WHITE);
    digits(s, p->them);
    font_draw(g, GFX_W - 2 - font_width(s, 1), 3, s, 1, GFX_WHITE);

    const uint32_t in = a->now - pk->t0;
    if (pk->point_at != 0 && !app_elapsed(a->now, pk->point_at, PONG_POINT_MS)) {
        char line[12];
        score_line(line, p->me, p->them);
        font_draw_centered(g, 64, 14, line, 3, GFX_WHITE);
        font_draw_centered(g, 64, 40, pk->point_mine ? "POINT!" : "OOPS", 1, GFX_WHITE);
    } else if (p->me == 0 && p->them == 0 && in < FIRST_HINT_MS) {
        font_draw_centered(g, 64, 24, "TILT TO MOVE", 1, GFX_WHITE);
    }

    if (p->phase == PONG_SERVE || p->phase == PONG_MINE) {
        gfx_fill_circle(g, pong_ball_px_x(p), pong_ball_px_y(p), PONG_BALL_R, GFX_WHITE);
    } else if (p->phase == PONG_AWAY && (a->now / 250) % 2 == 0) {
        const int ax = (int)(p->away_x >> 8);
        gfx_fill_triangle(g, ax, 3, ax - 2, 5, ax + 2, 5, GFX_WHITE);
    }
    gfx_fill_rrect(g, pong_paddle_px(p) - PONG_PADDLE_W / 2, PONG_PADDLE_Y, PONG_PADDLE_W, 3, 1, GFX_WHITE);
}

static const char *no_text(uint8_t why) {
    switch (why) {
        case BL_NO_BUSY: return "BIT IS BUSY";
        case BL_NO_DECLINED: return "BIT SAYS NO";
        case BL_NO_TIMEOUT: return "NO ANSWER";
        case TOSS_NO_BIT: return "NO BIT YET";
        default: return "BIT DIDN'T HEAR";
    }
}

void pongk_render(const app_t *a, gfx_t *g) {
    const pongk_t *p = &a->g.pong;
    switch (p->phase) {
        case PK_TITLE:
            art_game(g, ITEM_PONG);
            font_draw_centered(g, 64, 41, "VS BIT", 2, GFX_WHITE);
            app_hint(g, "ASK", NULL, "BACK");
            break;
        case PK_ASKING: {
            static const char *const DOTS[] = {"ASKING BIT", "ASKING BIT.", "ASKING BIT..", "ASKING BIT..."};
            font_draw(g, 34, 20, DOTS[(a->now / 400) % 4], 1, GFX_WHITE);
            font_draw_centered(g, 64, 34, "PRESS A ON BIT", 1, GFX_WHITE);
            app_hint(g, NULL, NULL, "CANCEL");
            break;
        }
        case PK_NO:
            font_draw_centered(g, 64, 24, no_text(p->why), 1, GFX_WHITE);
            break;
        case PK_PLAY:
            render_match(a, g);
            break;
        case PK_OVER: {
            const char *big = "BYE!";
            switch (p->end) {
                case PONG_END_WON: big = "YOU WIN!"; break;
                case PONG_END_LOST: big = "BIT WINS"; break;
                case PONG_END_THEY_QUIT: big = "BIT LEFT"; break;
                case PONG_END_GONE: big = "LOST BIT"; break;
                default: break;
            }
            font_draw_centered(g, 64, 6, big, 2, GFX_WHITE);
            char line[12];
            score_line(line, p->pong.me, p->pong.them);
            font_draw_centered(g, 64, 26, line, 2, GFX_WHITE);
            app_hint(g, "AGAIN", NULL, "BACK");
            break;
        }
        default:
            break;
    }
}

bool pongk_animating(const app_t *a) {
    const int ph = a->g.pong.phase;
    return ph == PK_PLAY || ph == PK_ASKING;
}
