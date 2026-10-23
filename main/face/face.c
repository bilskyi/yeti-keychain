#include "face/face.h"

#include <stdlib.h>

#include "config.h"

// ---------------------------------------------------------------------------
// Geometry. The face is drawn for a centred screen; the burn-in shift moves
// it by up to BURNIN_MAX_SHIFT_PX through the gfx offset. Everything below
// keeps a margin of at least that much plus the largest glance.
// ---------------------------------------------------------------------------
#define EYE_L_CX 38
#define EYE_R_CX 90
#define EYE_CY 28
#define MOUTH_CX 64
#define MOUTH_CY 52

#define GLANCE_X 12  // how far a glance moves the eyes sideways
#define GLANCE_Y 3

#define YAWN_MS 2600
#define TRANSITION_HOLD_MS 1200  // TAP: surprised for this long, then happy

#define OPEN_FULL 256  // eye openness is 0..256

typedef enum {
    EYE_ROUND,  // rounded rectangle, optionally lidded and/or slanted
    EYE_ARC,    // happy "^": the bottom is cut away by an ellipse
    EYE_HEART,
} eye_shape_t;

typedef enum {
    MOUTH_NONE,
    MOUTH_SMALL,  // short flat line
    MOUTH_SMILE,
    MOUTH_FROWN,
    MOUTH_O,
    MOUTH_BATTERY,  // empty battery outline, blinking
} mouth_t;

typedef struct {
    uint8_t w, h, r;
    uint8_t lid;    // 0..255 of the eye covered from the top
    uint8_t slant;  // px the inner top corner is cut down (angry brows)
    int8_t dy;      // eyes up (-) or down (+) from the centre line
    eye_shape_t shape;
    mouth_t mouth;
} style_t;

static const style_t STYLES[EXPR_COUNT] = {
    [EXPR_NEUTRAL]   = {.w = 26, .h = 30, .r = 8,  .shape = EYE_ROUND, .mouth = MOUTH_SMALL},
    [EXPR_HAPPY]     = {.w = 26, .h = 26, .r = 12, .shape = EYE_ARC,   .mouth = MOUTH_SMILE, .dy = -1},
    [EXPR_SLEEPY]    = {.w = 26, .h = 30, .r = 8,  .lid = 150, .dy = 2, .shape = EYE_ROUND, .mouth = MOUTH_SMALL},
    [EXPR_SURPRISED] = {.w = 30, .h = 34, .r = 15, .dy = -2, .shape = EYE_ROUND, .mouth = MOUTH_O},
    [EXPR_ANGRY]     = {.w = 26, .h = 26, .r = 6,  .slant = 11, .dy = 1, .shape = EYE_ROUND, .mouth = MOUTH_FROWN},
    [EXPR_LOVE]      = {.w = 30, .h = 28, .r = 0,  .shape = EYE_HEART, .mouth = MOUTH_SMILE},
    [EXPR_LOWBAT]    = {.w = 26, .h = 30, .r = 6,  .lid = 190, .dy = 3, .shape = EYE_ROUND, .mouth = MOUTH_BATTERY},
};

static const char *const NAMES[EXPR_COUNT] = {
    "neutral", "happy", "sleepy", "surprised", "angry", "love", "lowbat",
};

const char *face_expression_name(expression_t e) {
    return (unsigned)e < EXPR_COUNT ? NAMES[e] : "?";
}

// a has been reached by now, wrap-safe.
static bool reached(uint32_t now, uint32_t a) {
    return (int32_t)(now - a) >= 0;
}

// ---------------------------------------------------------------------------
// State changes
// ---------------------------------------------------------------------------

static void schedule_blink(face_t *f, uint32_t now) {
    const uint32_t lo = f->night ? BLINK_MAX_MS : BLINK_MIN_MS;
    f->next_blink = now + rng_range(&f->rng, lo, BLINK_MAX_MS * (f->night ? 2 : 1));
}

static void schedule_idle(face_t *f, uint32_t now) {
    f->next_idle = now + rng_range(&f->rng, IDLE_BEHAVIOR_MIN_MS, IDLE_BEHAVIOR_MAX_MS);
}

static void start_blink(face_t *f, uint32_t now) {
    f->blinking = true;
    f->blink_start = now;
}

void face_init(face_t *f, uint32_t seed, uint32_t now) {
    *f = (face_t){0};
    rng_seed(&f->rng, seed);
    f->expr = EXPR_NEUTRAL;
    f->expr_until = now + MOOD_MIN_MS;
    f->now = now;
    f->last_input = now;
    schedule_blink(f, now);
    schedule_idle(f, now);
}

void face_set_expression(face_t *f, expression_t e, uint32_t now, uint32_t hold_ms) {
    if ((unsigned)e >= EXPR_COUNT) return;
    f->expr_until = now + hold_ms;
    f->has_after = false;
    if (e == f->expr && !f->has_pending) return;
    // Swap at the bottom of a blink: the eyes close on one expression and
    // open on the next, which hides the change of shape.
    f->pending = e;
    f->has_pending = true;
    if (!f->blinking) start_blink(f, now);
}

void face_look(face_t *f, int dx, int dy, uint32_t now, uint32_t hold_ms) {
    f->look_tx = dx;
    f->look_ty = dy;
    f->look_until = hold_ms ? now + hold_ms : 0;
}

void face_set_night(face_t *f, bool night, uint32_t now) {
    if (night == f->night) return;
    f->night = night;
    f->idle = IDLE_NONE;
    face_look(f, 0, 0, now, 0);
    if (night) {
        f->reacting = false;
        face_set_expression(f, EXPR_SLEEPY, now, MOOD_MAX_MS);
    } else {
        // Woken up: a moment of surprise, then the mood machine.
        f->reacting = true;
        face_set_expression(f, EXPR_SURPRISED, now, TRANSITION_HOLD_MS);
    }
    schedule_blink(f, now);
    schedule_idle(f, now);
}

static void react(face_t *f, expression_t e, uint32_t now, uint32_t hold_ms) {
    f->reacting = true;
    f->idle = IDLE_NONE;
    face_set_expression(f, e, now, hold_ms);
}

void face_on_event(face_t *f, input_event_t ev, uint32_t now) {
    f->last_input = now;
    switch (ev) {
        case INPUT_TAP:
            react(f, EXPR_SURPRISED, now, TRANSITION_HOLD_MS);
            f->after = EXPR_HAPPY;
            f->has_after = true;
            break;
        case INPUT_DOUBLE_TAP:
            react(f, EXPR_LOVE, now, 4000);
            break;
        case INPUT_SHAKE:
            react(f, EXPR_ANGRY, now, 2500);
            break;
        case INPUT_TILT_LEFT:
        case INPUT_BTN_B_SHORT:
            face_look(f, -GLANCE_X, 0, now, 2000);
            break;
        case INPUT_TILT_RIGHT:
        case INPUT_BTN_C_SHORT:
            face_look(f, GLANCE_X, 0, now, 2000);
            break;
        case INPUT_BTN_A_SHORT: {
            // Cycle through every mood expression, mostly for testing faces.
            const expression_t base = f->has_pending ? f->pending : f->expr;
            react(f, (expression_t)((base + 1) % EXPR_MOOD_COUNT), now, 8000);
            break;
        }
        case INPUT_BTN_B_LONG:
        case INPUT_BTN_C_LONG:
            react(f, EXPR_HAPPY, now, 3000);
            break;
        case INPUT_BTN_A_LONG:  // the power policy uses this one (night toggle)
        case INPUT_NONE:
        case INPUT_EVENT_COUNT:
            break;
    }
}

static int ease_toward(int cur, int target) {
    const int d = target - cur;
    if (d == 0) return cur;
    int step = d / 3;
    if (step == 0) step = d > 0 ? 1 : -1;
    return cur + step;
}

static void start_idle(face_t *f, uint32_t now) {
    const bool yawn = f->expr == EXPR_SLEEPY ? rng_chance(&f->rng, 50)
                                             : rng_chance(&f->rng, 12);
    if (yawn) {
        f->idle = IDLE_YAWN;
        f->idle_start = now;
        return;
    }
    // Glance: mostly sideways, sometimes a little up or down.
    const int dir = rng_chance(&f->rng, 50) ? 1 : -1;
    const int dy = (int)rng_range(&f->rng, 0, 2 * GLANCE_Y) - GLANCE_Y;
    face_look(f, dir * (int)rng_range(&f->rng, GLANCE_X / 2, GLANCE_X), dy, now,
              rng_range(&f->rng, 700, 1600));
    f->idle = IDLE_GLANCE;
    f->idle_start = now;
}

void face_update(face_t *f, uint32_t now) {
    f->now = now;

    // Blink, and the expression swap at its bottom.
    if (f->blinking) {
        const uint32_t t = now - f->blink_start;
        if (f->has_pending && t >= BLINK_DURATION_MS / 2) {
            f->expr = f->pending;
            f->has_pending = false;
        }
        if (t >= BLINK_DURATION_MS) {
            f->blinking = false;
            if (f->double_blink) {
                f->double_blink = false;
                start_blink(f, now);
            } else {
                schedule_blink(f, now);
            }
        }
    } else if (reached(now, f->next_blink) && !f->night) {
        start_blink(f, now);
        f->double_blink = rng_chance(&f->rng, 20);
    } else if (reached(now, f->next_blink)) {
        schedule_blink(f, now);  // asleep: eyes stay shut, skip the blink
    }

    // Expression hold expired: follow-up reaction, or the mood machine.
    if (reached(now, f->expr_until) && !f->has_pending) {
        if (f->has_after) {
            f->has_after = false;
            face_set_expression(f, f->after, now, 4000);
        } else {
            f->reacting = false;
            const mood_ctx_t ctx = {
                .night = f->night,
                .ms_since_input = now - f->last_input,
            };
            uint32_t hold;
            const expression_t next = mood_pick(&f->rng, f->expr, &ctx, &hold);
            face_set_expression(f, next, now, hold);
        }
    }

    // Look: return to centre when the hold runs out, ease toward the target.
    if (f->look_until && reached(now, f->look_until)) {
        f->look_tx = 0;
        f->look_ty = 0;
        f->look_until = 0;
    }
    f->look_x = ease_toward(f->look_x, f->look_tx);
    f->look_y = ease_toward(f->look_y, f->look_ty);

    // Idle behaviours.
    switch (f->idle) {
        case IDLE_GLANCE:
            if (!f->look_until && f->look_x == 0 && f->look_y == 0) {
                f->idle = IDLE_NONE;
                schedule_idle(f, now);
            }
            break;
        case IDLE_YAWN:
            if (now - f->idle_start >= YAWN_MS) {
                f->idle = IDLE_NONE;
                schedule_idle(f, now);
            }
            break;
        case IDLE_NONE:
            if (reached(now, f->next_idle)) {
                if (f->night || f->reacting) {
                    schedule_idle(f, now);
                } else {
                    start_idle(f, now);
                }
            }
            break;
    }
}

bool face_is_animating(const face_t *f) {
    return f->blinking || f->has_pending || f->idle == IDLE_YAWN ||
           f->look_x != f->look_tx || f->look_y != f->look_ty;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

// 0..256: how far into the yawn the mouth is open. Opens, holds, closes.
static int yawn_amount(const face_t *f) {
    if (f->idle != IDLE_YAWN) return 0;
    const int t = (int)(f->now - f->idle_start);
    const int ramp = YAWN_MS * 3 / 10;
    if (t < ramp) return t * 256 / ramp;
    if (t > YAWN_MS - ramp) return (YAWN_MS - t) * 256 / ramp;
    return 256;
}

static int openness(const face_t *f) {
    int open = OPEN_FULL;
    if (f->blinking) {
        // Triangle: fully open -> shut at the midpoint -> open.
        const int t = (int)(f->now - f->blink_start);
        open = abs(OPEN_FULL - 2 * OPEN_FULL * t / BLINK_DURATION_MS);
        if (open > OPEN_FULL) open = OPEN_FULL;
    }
    // A yawn squeezes the eyes most of the way shut.
    open = open * (256 - yawn_amount(f) * 3 / 4) / 256;
    return open;
}

// Shut eye, sleeping: a shallow downward curve, 2 px thick.
static void draw_closed_eye(gfx_t *g, int cx, int cy, int w) {
    const int x0 = cx - w / 2, x1 = cx + w / 2 - 1;
    for (int t = 0; t < 2; t++) {
        gfx_line(g, x0, cy - 2 + t, x0 + 4, cy + t, GFX_WHITE);
        gfx_hline(g, x0 + 4, x1 - 4, cy + t, GFX_WHITE);
        gfx_line(g, x1 - 4, cy + t, x1, cy - 2 + t, GFX_WHITE);
    }
}

static void draw_heart(gfx_t *g, int cx, int cy, int s, int open) {
    // Two lobes and a point, squashed vertically while blinking.
    const int lr = s / 4 + 1;
    const int lry = lr * open / OPEN_FULL;
    const int lobe_y = cy - (s / 5) * open / OPEN_FULL;
    gfx_fill_ellipse(g, cx - s / 4, lobe_y, lr, lry, GFX_WHITE);
    gfx_fill_ellipse(g, cx + s / 4, lobe_y, lr, lry, GFX_WHITE);
    gfx_fill_triangle(g, cx - s / 2, lobe_y + lry / 3, cx + s / 2, lobe_y + lry / 3,
                      cx, cy + (s / 2) * open / OPEN_FULL, GFX_WHITE);
}

static void draw_eye(gfx_t *g, const style_t *s, int cx, int cy, int open, bool left) {
    const int h = s->h * open / OPEN_FULL;
    if (h < 3) {
        // Mid-blink: a straight line reads better than a 1-2 px sliver.
        gfx_fill_rect(g, cx - s->w / 2 + 2, cy - 1, s->w - 4, 2, GFX_WHITE);
        return;
    }
    if (s->shape == EYE_HEART) {
        draw_heart(g, cx, cy, s->w, open);
        return;
    }

    const int x = cx - s->w / 2;
    const int y = cy - h / 2;
    gfx_fill_rrect(g, x, y, s->w, h, s->r, GFX_WHITE);

    if (s->shape == EYE_ARC) {
        // Carve the bottom out with a wide ellipse, leaving a "^" arch.
        gfx_fill_ellipse(g, cx, y + h + h / 6, s->w / 2 + 5, h * 3 / 4, GFX_BLACK);
        return;
    }
    if (s->lid) {
        gfx_fill_rect(g, x - 1, y - 1, s->w + 2, 1 + h * s->lid / 256, GFX_BLACK);
    }
    if (s->slant) {
        // Cut the inner top corner: brows angled down toward the nose.
        const int sl = s->slant * open / OPEN_FULL;
        if (left) {
            gfx_fill_triangle(g, x - 1, y - 1, x + s->w, y - 1, x + s->w, y + sl, GFX_BLACK);
        } else {
            gfx_fill_triangle(g, x - 1, y - 1, x + s->w, y - 1, x - 1, y + sl, GFX_BLACK);
        }
    }
}

static void draw_mouth(gfx_t *g, mouth_t m, int cx, int cy, uint32_t now) {
    switch (m) {
        case MOUTH_NONE:
            break;
        case MOUTH_SMALL:
            gfx_fill_rect(g, cx - 4, cy, 9, 2, GFX_WHITE);
            break;
        case MOUTH_SMILE:
            // Lower half of a ring.
            gfx_fill_ellipse(g, cx, cy - 2, 7, 5, GFX_WHITE);
            gfx_fill_ellipse(g, cx, cy - 2, 5, 3, GFX_BLACK);
            gfx_fill_rect(g, cx - 8, cy - 8, 17, 6, GFX_BLACK);
            break;
        case MOUTH_FROWN:
            // Upper half of a ring.
            gfx_fill_ellipse(g, cx, cy + 4, 7, 5, GFX_WHITE);
            gfx_fill_ellipse(g, cx, cy + 4, 5, 3, GFX_BLACK);
            gfx_fill_rect(g, cx - 8, cy + 5, 17, 6, GFX_BLACK);
            break;
        case MOUTH_O:
            gfx_fill_ellipse(g, cx, cy, 4, 4, GFX_WHITE);
            gfx_fill_ellipse(g, cx, cy, 2, 2, GFX_BLACK);
            break;
        case MOUTH_BATTERY:
            // Empty battery, outline blinking at 1 Hz.
            if ((now / 500) % 2 == 0) {
                gfx_rect(g, cx - 8, cy - 4, 15, 9, GFX_WHITE);
                gfx_fill_rect(g, cx + 7, cy - 2, 2, 5, GFX_WHITE);
            }
            break;
    }
}

static void draw_yawn(gfx_t *g, int cx, int cy, int amount) {
    const int ry = 1 + 6 * amount / 256;
    gfx_fill_ellipse(g, cx, cy, 5, ry, GFX_WHITE);
    if (ry >= 3) gfx_fill_ellipse(g, cx, cy, 3, ry - 2, GFX_BLACK);
}

static void draw_z(gfx_t *g, int x, int y, int size) {
    gfx_hline(g, x, x + size, y, GFX_WHITE);
    gfx_line(g, x + size, y, x, y + size, GFX_WHITE);
    gfx_hline(g, x, x + size, y + size, GFX_WHITE);
}

// Two z's drifting up and to the right, the small one half a cycle behind.
static void draw_zzz(gfx_t *g, uint32_t now) {
    const int phase = (int)((now / 150) % 24);
    const int p2 = (phase + 12) % 24;
    draw_z(g, 108 + phase / 6, 16 - phase / 2, 5);
    draw_z(g, 106 + p2 / 6, 18 - p2 / 2, 3);
}

void face_render(const face_t *f, gfx_t *g) {
    const style_t *s = &STYLES[f->expr];

    if (f->night) {
        // Asleep: shut eyes light far fewer pixels than open ones, which is
        // also what we want at night for power.
        draw_closed_eye(g, EYE_L_CX, EYE_CY + 4, s->w);
        draw_closed_eye(g, EYE_R_CX, EYE_CY + 4, s->w);
        draw_zzz(g, f->now);
        return;
    }

    const int open = openness(f);
    const int ey = EYE_CY + s->dy + f->look_y;
    draw_eye(g, s, EYE_L_CX + f->look_x, ey, open, true);
    draw_eye(g, s, EYE_R_CX + f->look_x, ey, open, false);

    const int mx = MOUTH_CX + f->look_x / 2;
    const int my = MOUTH_CY + f->look_y / 2;
    const int yawn = yawn_amount(f);
    if (yawn) {
        draw_yawn(g, mx, my, yawn);
    } else {
        draw_mouth(g, s->mouth, mx, my, f->now);
    }

    if (f->expr == EXPR_SLEEPY) draw_zzz(g, f->now);
}
