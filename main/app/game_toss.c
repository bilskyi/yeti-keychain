// BIT: throw BIT something. B picks a heart, a star or a bomb; A - or a
// flick of the keychain - throws it. It flies off the top of the screen and
// lands on BIT's, and BIT says how it took it: all soft for a heart,
// starry-eyed for a star, knocked out by a bomb.
//
// Over the radio (link/bitlink.h): a THROW that is resent until BIT's radio
// takes it, then BIT's CAUGHT once the thing has landed over there.

#include "app/app_internal.h"

#define FLY_MS 380       // up and off the screen
#define ANSWER_MS 4000   // delivered, BIT's answer should come within this
#define RESULT_MS 2600

void app_toss_item(gfx_t *g, int item, int cx, int cy, int s) {
    switch (item) {
        case BL_ITEM_HEART:
            app_heart(g, cx, cy, s);
            break;
        case BL_ITEM_STAR: {
            // Five points: a triangle up, and two leaning ones under it.
            const int r = s / 2, h = s * 3 / 10;
            gfx_fill_triangle(g, cx, cy - r, cx - h, cy + h / 2, cx + h, cy + h / 2, GFX_WHITE);
            gfx_fill_triangle(g, cx - r, cy - h / 3, cx + r, cy - h / 3, cx - r * 3 / 5, cy + r, GFX_WHITE);
            gfx_fill_triangle(g, cx - r, cy - h / 3, cx + r, cy - h / 3, cx + r * 3 / 5, cy + r, GFX_WHITE);
            break;
        }
        default: {  // a bomb: a ball, a cap, a fuse
            const int r = s * 2 / 5;
            gfx_fill_circle(g, cx, cy + s / 10, r, GFX_WHITE);
            gfx_fill_rect(g, cx + r / 2 - 2, cy + s / 10 - r - 2, 5, 4, GFX_WHITE);
            gfx_line(g, cx + r / 2, cy + s / 10 - r - 2, cx + r / 2 + 4, cy + s / 10 - r - 6, GFX_WHITE);
            gfx_pixel(g, cx - r / 3, cy - r / 3, GFX_BLACK);  // a glint
            gfx_pixel(g, cx - r / 3 - 1, cy - r / 3 + 1, GFX_BLACK);
            break;
        }
    }
}

static const char *item_name(int item) {
    switch (item) {
        case BL_ITEM_HEART: return "HEART";
        case BL_ITEM_STAR: return "STAR";
        default: return "BOMB";
    }
}

static void begin(toss_t *t, int phase, uint32_t now) {
    t->phase = phase;
    t->t0 = now;
}

static void result(app_t *a, uint8_t r, uint32_t now) {
    toss_t *t = &a->g.toss;
    t->result = r;
    begin(t, TOSS_RESULT, now);
    switch (r) {
        case BL_CAUGHT_LOVED: app_play(a, SND_SPARKLE); a->mood = EXPR_LOVE; break;
        case BL_CAUGHT_WOW: app_play(a, SND_DING); a->mood = EXPR_EXCITED; break;
        case BL_CAUGHT_KO: app_play(a, SND_ROFL); a->mood = EXPR_LAUGH; break;
        default: app_play(a, SND_FAIL); break;
    }
}

void toss_start(app_t *a, uint32_t now) {
    toss_t *t = &a->g.toss;
    t->item = BL_ITEM_HEART;
    begin(t, TOSS_AIM, now);
}

static void throw_it(app_t *a, uint32_t now) {
    toss_t *t = &a->g.toss;
    const app_link_t st = app_link_status(a);
    if (st == APP_LINK_OFF || st == APP_LINK_SEARCHING) {
        result(a, TOSS_NO_BIT, now);
        return;
    }
    const bl_msg_t m = {.type = BL_THROW, .d = (uint8_t)t->item};
    if (!app_link_send(a, &m, true)) {
        result(a, TOSS_NO_BIT, now);
        return;
    }
    t->delivered = false;
    begin(t, TOSS_FLYING, now);
    app_play(a, SND_WHEE);
}

bool toss_event(app_t *a, input_event_t ev, uint32_t now) {
    toss_t *t = &a->g.toss;
    if (t->phase != TOSS_AIM) return ev != INPUT_BTN_C_SHORT;  // in flight: only back
    switch (ev) {
        case INPUT_BTN_A_SHORT:
        case INPUT_SHAKE:  // a flick of the wrist
            throw_it(a, now);
            return true;
        case INPUT_BTN_B_SHORT:
            t->item = t->item % (BL_ITEM_COUNT - 1) + 1;
            app_play(a, SND_MOVE);
            return true;
        default:
            return false;
    }
}

void toss_link(app_t *a, const bl_msg_t *m, uint32_t now) {
    toss_t *t = &a->g.toss;
    if (m->type != BL_CAUGHT || (t->phase != TOSS_FLYING && t->phase != TOSS_WAIT)) return;
    result(a, m->e, now);
}

void toss_link_done(app_t *a, const bl_msg_t *m, bool delivered, uint32_t now) {
    toss_t *t = &a->g.toss;
    if (m->type != BL_THROW || (t->phase != TOSS_FLYING && t->phase != TOSS_WAIT)) return;
    if (delivered) {
        t->delivered = true;
    } else {
        result(a, TOSS_MISSED, now);
    }
}

void toss_update(app_t *a, uint32_t now) {
    toss_t *t = &a->g.toss;
    switch (t->phase) {
        case TOSS_FLYING:
            if (app_elapsed(now, t->t0, FLY_MS)) begin(t, TOSS_WAIT, now);
            break;
        case TOSS_WAIT:
            // Taken, but no word of how it went: it got there, that is all.
            if (app_elapsed(now, t->t0, ANSWER_MS + BL_GIVE_UP_MS)) result(a, t->delivered ? 0 : TOSS_MISSED, now);
            break;
        case TOSS_RESULT:
            if (app_elapsed(now, t->t0, RESULT_MS)) begin(t, TOSS_AIM, now);
            break;
        default:
            break;
    }
}

static void status_line(const app_t *a, gfx_t *g) {
    const char *s = "NO RADIO";
    switch (app_link_status(a)) {
        case APP_LINK_NEAR: s = "BIT IS NEAR"; break;
        case APP_LINK_AWAY: s = "BIT IS AWAY"; break;
        case APP_LINK_SEARCHING: {
            static const char *const DOTS[] = {"LOOKING", "LOOKING.", "LOOKING..", "LOOKING..."};
            s = DOTS[(a->now / 400) % 4];
            break;
        }
        default: break;
    }
    font_draw_centered(g, 64, 0, s, 1, GFX_WHITE);
}

static const char *result_text(uint8_t r) {
    switch (r) {
        case BL_CAUGHT_LOVED: return "BIT LOVES IT";
        case BL_CAUGHT_WOW: return "BIT: OOH!";
        case BL_CAUGHT_KO: return "KO! HEHE";
        case BL_CAUGHT_BUSY: return "BIT IS BUSY";
        case TOSS_MISSED: return "MISSED!";
        case TOSS_NO_BIT: return "NO BIT YET";
        default: return "CAUGHT!";
    }
}

void toss_render(const app_t *a, gfx_t *g) {
    const toss_t *t = &a->g.toss;
    const uint32_t in = a->now - t->t0;
    switch (t->phase) {
        case TOSS_AIM: {
            status_line(a, g);
            // Bobbing a little, ready to go.
            const int bob = app_sin64((int)(a->now / 30)) * 2 / 256;
            app_toss_item(g, t->item, 64, 27 + bob, 26);
            font_draw_centered(g, 64, 45, item_name(t->item), 1, GFX_WHITE);
            app_hint(g, "THROW", "NEXT", "BACK");
            break;
        }
        case TOSS_FLYING: {
            // Up and away, shrinking, with a trail.
            const int k = (int)(in * 256 / FLY_MS);
            const int y = 27 - (27 + 20) * k * k / (256 * 256);
            const int s = 26 - 12 * k / 256;
            for (int i = 1; i <= 3; i++) gfx_hline(g, 64 - 6 + i * 3, 64 - 5 + i * 3, y + s / 2 + i * 4, GFX_WHITE);
            app_toss_item(g, t->item, 64, y, s);
            break;
        }
        case TOSS_WAIT:
            status_line(a, g);
            font_draw_centered(g, 64, 24, (a->now / 300) % 2 ? "TO BIT..." : "TO BIT.. ", 1, GFX_WHITE);
            break;
        case TOSS_RESULT: {
            const uint8_t r = t->result;
            if (r == BL_CAUGHT_LOVED) {  // little hearts drifting up
                for (int i = 0; i < 5; i++) {
                    const int y = 40 - (int)((in / 25 + (uint32_t)i * 9) % 44);
                    app_heart(g, 14 + i * 25, y, 8);
                }
            } else if (r == BL_CAUGHT_KO) {  // the bang, over there
                const int rr = 6 + (int)(in / 40) % 24;
                gfx_fill_circle(g, 64, 18, rr, GFX_WHITE);
                gfx_fill_circle(g, 64, 18, rr > 4 ? rr - 4 : 0, GFX_BLACK);
            } else if (r == TOSS_MISSED || r == TOSS_NO_BIT) {  // it fell back down
                const int y = in < 400 ? -10 + (int)(in * 40 / 400) : 30;
                app_toss_item(g, t->item, 64, y, 16);
            }
            font_draw_centered(g, 64, 44, result_text(r), 1, GFX_WHITE);
            break;
        }
        default:
            break;
    }
}

bool toss_animating(const app_t *a) {
    (void)a;
    return true;  // the item bobs while it waits to go
}
