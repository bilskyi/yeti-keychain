// Looking after the pet: feeding it, its stats page, and the accelerometer
// calibration (so tilt means the same thing whichever way the board sits in
// the case).

#include "app/app_internal.h"
#include "input/orient.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Feed: B picks a food, A feeds it. The Yeti chomps it in three bites.
// ---------------------------------------------------------------------------

#define EAT_MS 1500
#define BITE_MS 400  // first bite at FLY_MS, then every BITE_MS
#define FLY_MS 300
#define YUM_MS 1300
#define FULL_MS 1300

static const struct {
    const char *name;
    int food, fun;
} FOODS[FOOD_KINDS] = {
    {"FISH", 35, 0},
    {"BERRY", 12, 4},
    {"CAKE", 20, 12},
};

static void draw_food(gfx_t *g, int kind, int cx, int cy) {
    switch (kind) {
        case 0:  // fish
            gfx_fill_ellipse(g, cx - 2, cy, 9, 6, GFX_WHITE);
            gfx_fill_triangle(g, cx + 5, cy, cx + 12, cy - 7, cx + 12, cy + 7, GFX_WHITE);
            gfx_fill_circle(g, cx - 6, cy - 2, 1, GFX_BLACK);
            break;
        case 1:  // a bunch of berries with a leaf
            gfx_fill_circle(g, cx - 4, cy + 1, 4, GFX_WHITE);
            gfx_fill_circle(g, cx + 4, cy + 1, 4, GFX_WHITE);
            gfx_fill_circle(g, cx, cy + 6, 4, GFX_WHITE);
            gfx_pixel(g, cx - 5, cy, GFX_BLACK);
            gfx_pixel(g, cx + 3, cy, GFX_BLACK);
            gfx_line(g, cx, cy - 3, cx + 2, cy - 9, GFX_WHITE);
            gfx_fill_triangle(g, cx + 2, cy - 9, cx + 9, cy - 9, cx + 5, cy - 5, GFX_WHITE);
            break;
        case 2:  // a slice of cake with a cherry
            gfx_fill_triangle(g, cx - 10, cy - 3, cx + 10, cy - 7, cx + 10, cy - 3, GFX_WHITE);
            gfx_fill_rect(g, cx - 10, cy - 3, 21, 11, GFX_WHITE);
            gfx_hline(g, cx - 9, cx + 9, cy + 2, GFX_BLACK);
            gfx_fill_circle(g, cx + 5, cy - 10, 2, GFX_WHITE);
            break;
        default:  // back arrow
            gfx_fill_triangle(g, cx - 12, cy, cx - 2, cy - 8, cx - 2, cy + 8, GFX_WHITE);
            gfx_fill_rect(g, cx - 3, cy - 3, 14, 7, GFX_WHITE);
            break;
    }
}

void feed_start(app_t *a, uint32_t now) {
    a->g.feed.phase = FEED_PICK;
    a->g.feed.choice = 0;
    a->g.feed.t0 = now;
}

bool feed_event(app_t *a, input_event_t ev, uint32_t now) {
    feed_t *f = &a->g.feed;
    if (f->phase != FEED_PICK) return ev == INPUT_BTN_A_SHORT || ev == INPUT_BTN_B_SHORT;
    if (ev == INPUT_BTN_B_SHORT) {
        f->choice = (f->choice + 1) % (FOOD_KINDS + 1);
        app_play(a, SND_MOVE);
        return true;
    }
    if (ev != INPUT_BTN_A_SHORT) return false;
    if (f->choice == FOOD_KINDS) {  // BACK: let the menu have it
        app_play(a, SND_BACK);
        a->mode = APP_MENU;
        return true;
    }
    f->t0 = now;
    f->bites = 0;
    if (pet_feed(&a->save.pet, FOODS[f->choice].food, FOODS[f->choice].fun)) {
        f->phase = FEED_EAT;
    } else {
        f->phase = FEED_FULL;
        app_play(a, SND_BACK);
    }
    return true;
}

// Bites taken by t ms into eating (0..3).
static int bites(uint32_t t) {
    if (t < FLY_MS) return 0;
    const int b = 1 + (int)((t - FLY_MS) / BITE_MS);
    return b > 3 ? 3 : b;
}

void feed_update(app_t *a, uint32_t now) {
    feed_t *f = &a->g.feed;
    const uint32_t t = now - f->t0;
    switch (f->phase) {
        case FEED_EAT: {
            const int n = bites(t);
            if (n > f->bites) {  // a chomp for each bite
                f->bites = n;
                app_play(a, SND_EAT);
            }
            if (t >= EAT_MS) {
                f->phase = FEED_YUM;
                f->t0 = now;
                a->mood = EXPR_LOVE;
                app_play(a, SND_DING);
                if (a->env && a->env->save) a->env->save(&a->save);
                app_reward(a, 0, 0, 2);
            }
            break;
        }
        case FEED_YUM:
            if (t >= YUM_MS) f->phase = FEED_PICK;
            break;
        case FEED_FULL:
            if (t >= FULL_MS) f->phase = FEED_PICK;
            break;
        case FEED_PICK:
            break;
    }
}

// The Yeti, big, eating: dot eyes, mouth opening and shutting.
static void eater(gfx_t *g, uint32_t t, int kind) {
    const bool open = t >= FLY_MS - 100 && ((t - (FLY_MS - 100)) / 130) % 3 != 2;
    gfx_fill_rrect(g, 27, 10, 14, 22, 6, GFX_WHITE);  // pill eyes
    gfx_fill_rrect(g, 75, 10, 14, 22, 6, GFX_WHITE);
    if (open) {
        gfx_fill_ellipse(g, 58, 42, 9, 7, GFX_WHITE);
        gfx_fill_ellipse(g, 58, 42, 6, 4, GFX_BLACK);
    } else {
        gfx_fill_rect(g, 50, 41, 17, 3, GFX_WHITE);
    }
    // The food flies in from the right, then gets smaller with each bite.
    const int n = bites(t);
    if (n >= 3) return;
    int fx = 80;
    if (t < FLY_MS) fx = 130 - (int)t * 50 / FLY_MS;
    draw_food(g, kind, fx + 12, 42);
    // Bite marks: black circles eating it from the left.
    for (int i = 0; i < n; i++) gfx_fill_circle(g, fx + 2 + i * 7, 40 + (i % 2) * 4, 6, GFX_BLACK);
    // Crumbs falling below the mouth.
    if (n > 0) {
        const int c = (int)((t - FLY_MS) % BITE_MS) / 40;
        gfx_pixel(g, 70 + c / 2, 50 + c, GFX_WHITE);
        gfx_pixel(g, 74 - c / 3, 52 + c, GFX_WHITE);
    }
}

void feed_render(const app_t *a, gfx_t *g) {
    const feed_t *f = &a->g.feed;
    const uint32_t t = a->now - f->t0;
    switch (f->phase) {
        case FEED_PICK: {
            draw_food(g, f->choice, 64, 14);
            font_draw_centered(g, 64, 27, f->choice == FOOD_KINDS ? "BACK" : FOODS[f->choice].name, 2,
                               GFX_WHITE);
            font_draw(g, 8, 46, "FOOD", 1, GFX_WHITE);
            app_bar(g, 36, 45, 84, 9, a->save.pet.food, PET_MAX);
            app_hint(g, f->choice == FOOD_KINDS ? "A:OK" : "A:FEED", "B:NEXT");
            break;
        }
        case FEED_EAT:
            eater(g, t, f->choice);
            break;
        case FEED_YUM: {
            // Happy ^ ^ eyes, YUM!, hearts floating up.
            for (int k = 0; k < 3; k++) {
                gfx_line(g, 26, 26 + k, 34, 18 + k, GFX_WHITE);
                gfx_line(g, 34, 18 + k, 42, 26 + k, GFX_WHITE);
                gfx_line(g, 86, 26 + k, 94, 18 + k, GFX_WHITE);
                gfx_line(g, 94, 18 + k, 102, 26 + k, GFX_WHITE);
            }
            font_draw_centered(g, 64, 36, "YUM!", 2, GFX_WHITE);
            const int rise = (int)(t / 40);
            app_heart(g, 16, 40 - rise % 36, 7);
            app_heart(g, 112, 52 - (rise + 18) % 36, 7);
            break;
        }
        case FEED_FULL: {
            // Squeezed > < eyes, head shaking no.
            const int dx = (t / 90) % 2 ? 3 : -3;
            for (int k = 0; k < 3; k++) {
                gfx_line(g, 28 + dx + k, 14, 40 + dx + k, 21, GFX_WHITE);
                gfx_line(g, 40 + dx + k, 21, 28 + dx + k, 28, GFX_WHITE);
                gfx_line(g, 100 + dx + k, 14, 88 + dx + k, 21, GFX_WHITE);
                gfx_line(g, 88 + dx + k, 21, 100 + dx + k, 28, GFX_WHITE);
            }
            font_draw_centered(g, 64 + dx, 38, "I'M FULL!", 2, GFX_WHITE);
            break;
        }
    }
}

bool feed_animating(const app_t *a) {
    return a->g.feed.phase != FEED_PICK;
}

// ---------------------------------------------------------------------------
// Stats: how the pet is doing.
// ---------------------------------------------------------------------------

void stats_start(app_t *a, uint32_t now) {
    (void)a;
    (void)now;
}

bool stats_event(app_t *a, input_event_t ev, uint32_t now) {
    (void)a;
    (void)now;
    return ev == INPUT_BTN_A_SHORT;  // nothing to do; B goes back
}

void stats_update(app_t *a, uint32_t now) {
    (void)a;
    (void)now;
}

void stats_render(const app_t *a, gfx_t *g) {
    const pet_t *p = &a->save.pet;
    char line[20] = "LV ";
    app_itoa(p->level, line + 3);
    font_draw(g, 2, 2, line, 1, GFX_WHITE);
    // Age: hours for the first day, then days.
    char age[20] = "AGE ";
    const uint32_t h = p->minutes / 60;
    app_itoa((int)(h < 24 ? h : h / 24), age + 4);
    const int n = (int)strlen(age);
    age[n] = h < 24 ? 'H' : 'D';
    age[n + 1] = 0;
    font_draw(g, GFX_W - 2 - font_width(age, 1), 2, age, 1, GFX_WHITE);

    static const char *const LABELS[4] = {"FOOD", "FUN", "REST", "XP"};
    const int values[4] = {p->food, p->fun, p->energy, p->xp};
    const int maxes[4] = {PET_MAX, PET_MAX, PET_MAX, pet_xp_next(p->level)};
    for (int i = 0; i < 4; i++) {
        const int y = 14 + i * 12;
        font_draw(g, 2, y + 1, LABELS[i], 1, GFX_WHITE);
        app_bar(g, 32, y, 94, 9, values[i], maxes[i]);
        // A low stat blinks its label, like the face's thought bubble.
        if (i < 3 && values[i] < PET_LOW && (a->now / 500) % 2) gfx_fill_rect(g, 2, y + 1, 28, 7, GFX_BLACK);
    }
}

bool stats_animating(const app_t *a) {
    const pet_t *p = &a->save.pet;
    return p->food < PET_LOW || p->fun < PET_LOW || p->energy < PET_LOW;  // the blink
}

// ---------------------------------------------------------------------------
// Setup: calibrate the accelerometer in two steps.
// ---------------------------------------------------------------------------

void setup_start(app_t *a, uint32_t now) {
    a->g.setup.phase = SETUP_FLAT;
    a->g.setup.t0 = now;
}

bool setup_event(app_t *a, input_event_t ev, uint32_t now) {
    setup_t *s = &a->g.setup;
    if (ev != INPUT_BTN_A_SHORT) return false;
    int mg[3] = {0, 0, 0};
    const bool ok = a->env && a->env->accel && a->env->accel(mg);
    s->t0 = now;
    switch (s->phase) {
        case SETUP_FLAT: {
            // Must already be clearly along one axis (orient_solve checks again).
            bool clear = false;
            for (int i = 0; i < 3; i++) {
                s->flat[i] = mg[i];
                if (mg[i] > 700 || mg[i] < -700) clear = true;
            }
            s->phase = ok && clear ? SETUP_UPRIGHT : SETUP_FAIL;
            app_play(a, s->phase == SETUP_UPRIGHT ? SND_SELECT : SND_FAIL);
            break;
        }
        case SETUP_UPRIGHT: {
            int8_t map[3];
            if (ok && orient_solve(s->flat, mg, map)) {
                for (int i = 0; i < 3; i++) a->save.orient[i] = map[i];
                if (a->env->orient) a->env->orient(map);
                if (a->env->save) a->env->save(&a->save);
                s->phase = SETUP_DONE;
                app_play(a, SND_RECORD);
            } else {
                s->phase = SETUP_FAIL;
                app_play(a, SND_FAIL);
            }
            break;
        }
        case SETUP_DONE:
        case SETUP_FAIL:
            s->phase = SETUP_FLAT;
            app_play(a, SND_SELECT);
            break;
    }
    return true;
}

void setup_update(app_t *a, uint32_t now) {
    (void)a;
    (void)now;
}

void setup_render(const app_t *a, gfx_t *g) {
    const setup_t *s = &a->g.setup;
    switch (s->phase) {
        case SETUP_FLAT:
            font_draw_centered(g, 64, 2, "1/2", 1, GFX_WHITE);
            font_draw_centered(g, 64, 16, "LAY ME FLAT", 1, GFX_WHITE);
            font_draw_centered(g, 64, 26, "SCREEN UP", 1, GFX_WHITE);
            gfx_fill_rect(g, 40, 40, 48, 4, GFX_WHITE);
            app_hint(g, "A:OK", "B:BACK");
            break;
        case SETUP_UPRIGHT:
            font_draw_centered(g, 64, 2, "2/2", 1, GFX_WHITE);
            font_draw_centered(g, 64, 16, "HOLD ME UPRIGHT", 1, GFX_WHITE);
            font_draw_centered(g, 64, 26, "LIKE NORMAL", 1, GFX_WHITE);
            gfx_fill_rect(g, 62, 36, 4, 14, GFX_WHITE);
            app_hint(g, "A:OK", "B:BACK");
            break;
        case SETUP_DONE: {
            font_draw_centered(g, 64, 4, "DONE!", 2, GFX_WHITE);
            font_draw_centered(g, 64, 24, "TILT ME", 1, GFX_WHITE);
            // A spirit level to check it: the bubble goes to the high side.
            int mg = 0;
            if (a->env && a->env->tilt) a->env->tilt(&mg);
            if (mg > 500) mg = 500;
            if (mg < -500) mg = -500;
            gfx_fill_rrect(g, 14, 36, 100, 14, 7, GFX_WHITE);
            gfx_fill_rrect(g, 16, 38, 96, 10, 5, GFX_BLACK);
            gfx_fill_rect(g, 56, 38, 1, 10, GFX_WHITE);
            gfx_fill_rect(g, 72, 38, 1, 10, GFX_WHITE);
            gfx_fill_circle(g, 64 - mg * 42 / 500, 42, 4, GFX_WHITE);
            app_hint(g, "A:AGAIN", "B:BACK");
            break;
        }
        case SETUP_FAIL:
            font_draw_centered(g, 64, 8, "HMM...", 2, GFX_WHITE);
            font_draw_centered(g, 64, 28, "HOLD STILL AND", 1, GFX_WHITE);
            font_draw_centered(g, 64, 38, "TRY AGAIN", 1, GFX_WHITE);
            app_hint(g, "A:AGAIN", "B:BACK");
            break;
    }
}

bool setup_animating(const app_t *a) {
    return a->g.setup.phase == SETUP_DONE;
}
