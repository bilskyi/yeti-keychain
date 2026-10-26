// Dice and Coin: the two games of chance. Shake, tap or press A.

#include "app/app_internal.h"

// ---------------------------------------------------------------------------
// Dice
// ---------------------------------------------------------------------------

#define ROLL_MS 800
#define DIE 40  // px

static bool is_throw(input_event_t ev) {
    return ev == INPUT_SHAKE || ev == INPUT_TAP || ev == INPUT_DOUBLE_TAP || ev == INPUT_BTN_A_SHORT;
}

void dice_start(app_t *a, uint32_t now) {
    dice_t *d = &a->g.dice;
    d->phase = DICE_IDLE;
    d->value = d->shown = 6;
    d->t0 = now;
}

bool dice_event(app_t *a, input_event_t ev, uint32_t now) {
    dice_t *d = &a->g.dice;
    if (!is_throw(ev) || d->phase == DICE_ROLLING) return false;
    d->phase = DICE_ROLLING;
    d->t0 = now;
    d->next_flip = now;
    d->value = (int)rng_range(&a->rng, 1, 6);
    app_play(a, SND_ROLL);
    return true;
}

void dice_update(app_t *a, uint32_t now) {
    dice_t *d = &a->g.dice;
    if (d->phase != DICE_ROLLING) return;
    if (app_elapsed(now, d->t0, ROLL_MS)) {
        d->phase = DICE_SHOWN;
        d->shown = d->value;
        app_play(a, SND_DING);
        app_reward(a, 1, 0, 1);
        return;
    }
    if (app_elapsed(now, d->next_flip, 0)) {
        // Tumble: a different face each flip, slowing down toward the end.
        int f;
        do {
            f = (int)rng_range(&a->rng, 1, 6);
        } while (f == d->shown);
        d->shown = f;
        d->next_flip = now + 50 + (now - d->t0) / 6;
    }
}

static void die_face(gfx_t *g, int cx, int cy, int v) {
    gfx_fill_rrect(g, cx - DIE / 2, cy - DIE / 2, DIE, DIE, 7, GFX_WHITE);
    const int o = 11;  // pip offset from the centre
    // Which of the 7 pip spots are set for each value: TL TR ML C MR BL BR.
    static const uint8_t SPOTS[7] = {0, 0x08, 0x41, 0x49, 0x63, 0x6B, 0x77};
    static const int8_t POS[7][2] = {{-1, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {1, 1}};
    for (int i = 0; i < 7; i++) {
        if (SPOTS[v] & (1u << (6 - i))) gfx_fill_circle(g, cx + POS[i][0] * o, cy + POS[i][1] * o, 3, GFX_BLACK);
    }
}

void dice_render(const app_t *a, gfx_t *g) {
    const dice_t *d = &a->g.dice;
    int cx = 64, cy = 27;
    if (d->phase == DICE_ROLLING) {
        // Rattle around while rolling (a cheap hash of time: render stays const).
        const uint32_t k = a->now / 40;
        cx += (int)((k * 7) % 9) - 4;
        cy += (int)((k * 5) % 5) - 2;
    }
    die_face(g, cx, cy, d->shown);
    if (d->phase == DICE_IDLE) app_hint(g, "A/SHAKE", "B:BACK");
    if (d->phase == DICE_SHOWN) {
        char n[4];
        app_itoa(d->value, n);
        font_draw(g, 96, 20, n, 2, GFX_WHITE);
    }
}

bool dice_animating(const app_t *a) {
    return a->g.dice.phase == DICE_ROLLING;
}

// ---------------------------------------------------------------------------
// Coin
// ---------------------------------------------------------------------------

#define FLIP_MS 1200
#define COIN_R 17
#define FLIP_TURNS 5  // half-turns shown while in the air (odd keeps the start side)

void coin_start(app_t *a, uint32_t now) {
    coin_t *c = &a->g.coin;
    c->phase = COIN_IDLE;
    c->heads = true;
    c->t0 = now;
}

bool coin_event(app_t *a, input_event_t ev, uint32_t now) {
    coin_t *c = &a->g.coin;
    if (!is_throw(ev) || c->phase == COIN_FLIP) return false;
    c->phase = COIN_FLIP;
    c->t0 = now;
    c->heads = rng_chance(&a->rng, 50);
    app_play(a, SND_FLAP);
    return true;
}

void coin_update(app_t *a, uint32_t now) {
    coin_t *c = &a->g.coin;
    if (c->phase == COIN_FLIP && app_elapsed(now, c->t0, FLIP_MS)) {
        c->phase = COIN_SHOWN;
        app_play(a, SND_COIN);
        app_reward(a, 1, 0, 1);
    }
}

// The coin seen edge-on by `w256` (256 = face on); heads shows the Yeti face,
// tails a snowflake.
static void coin_draw(gfx_t *g, int cx, int cy, int w256, bool heads) {
    const int rx = COIN_R * w256 / 256;
    if (rx < 2) {
        gfx_fill_rect(g, cx - 1, cy - COIN_R, 3, 2 * COIN_R + 1, GFX_WHITE);
        return;
    }
    gfx_fill_ellipse(g, cx, cy, rx, COIN_R, GFX_WHITE);
    if (rx > 4) gfx_fill_ellipse(g, cx, cy, rx - 2, COIN_R - 2, GFX_BLACK);
    if (rx < 8) return;
    if (heads) {
        const int s = 7 * rx / COIN_R;
        gfx_fill_circle(g, cx - s, cy - 2, 2, GFX_WHITE);
        gfx_fill_circle(g, cx + s, cy - 2, 2, GFX_WHITE);
        for (int x = -s / 2 - 1; x <= s / 2 + 1; x++) gfx_fill_rect(g, cx + x, cy + 6 - (x * x) / 8, 1, 2, GFX_WHITE);
    } else {
        const int s = 9 * rx / COIN_R;
        gfx_line(g, cx - s, cy, cx + s, cy, GFX_WHITE);
        gfx_line(g, cx - s / 2, cy - 9, cx + s / 2, cy + 9, GFX_WHITE);
        gfx_line(g, cx + s / 2, cy - 9, cx - s / 2, cy + 9, GFX_WHITE);
        gfx_fill_circle(g, cx, cy, 2, GFX_WHITE);
    }
}

void coin_render(const app_t *a, gfx_t *g) {
    const coin_t *c = &a->g.coin;
    if (c->phase == COIN_FLIP) {
        const int t = (int)(a->now - c->t0);
        // Angle in 1/64 turns: FLIP_TURNS half-turns over the flight.
        const int ang = t * FLIP_TURNS * 32 / FLIP_MS;
        const int cosv = app_sin64(ang + 16);
        const int w = cosv < 0 ? -cosv : cosv;
        // Side showing: the result on the last half-turn, alternating before.
        const int half = ang / 32;
        const bool heads = ((FLIP_TURNS - 1 - half) % 2 == 0) ? c->heads : !c->heads;
        const int lift = 14 * app_sin64(t * 32 / FLIP_MS) / 256;  // up and back down
        coin_draw(g, 64, 28 - lift, w, heads);
        return;
    }
    coin_draw(g, 64, 24, 256, c->heads);
    if (c->phase == COIN_IDLE) {
        app_hint(g, "A/TAP", "B:BACK");
    } else {
        font_draw_centered(g, 64, 47, c->heads ? "HEADS" : "TAILS", 2, GFX_WHITE);
    }
}

bool coin_animating(const app_t *a) {
    return a->g.coin.phase == COIN_FLIP;
}
