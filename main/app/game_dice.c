// Dice: shake it, tap it or press A.

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
    if (d->phase != DICE_ROLLING) app_hint(g, "ROLL", NULL, "BACK");
    if (d->phase == DICE_SHOWN) {
        char n[4];
        app_itoa(d->value, n);
        font_draw(g, 96, 20, n, 2, GFX_WHITE);
    }
}

bool dice_animating(const app_t *a) {
    return a->g.dice.phase == DICE_ROLLING;
}
