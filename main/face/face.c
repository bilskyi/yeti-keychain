#include "face/face.h"

#include <stdlib.h>

#include "config.h"

// ---------------------------------------------------------------------------
// Geometry: the "Mochi dots" look. Two small round eyes set wide apart and a
// bold little smile just under eye level, all near one line (Sasha's
// reference photo, 2026-10-25). The face is drawn for a centred screen; the
// burn-in shift moves it by up to BURNIN_MAX_SHIFT_PX through the gfx offset,
// and everything below keeps a margin of at least that plus the largest
// glance.
// ---------------------------------------------------------------------------
#define EYE_L_CX 34
#define EYE_R_CX 94
#define EYE_CY 30
#define EYE_R 10  // 7 was too small on the real panel (2026-10-25)
#define MOUTH_CX 64
#define MOUTH_CY 36
#define STROKE 3  // line weight of the smile and of ^ > < eyes

#define GLANCE_X 12  // how far a glance moves the eyes sideways
#define GLANCE_Y 3

#define YAWN_MS 2600
#define TRANSITION_HOLD_MS 1200  // TAP: surprised for this long, then happy
#define DIZZY_MS 2500
#define KO_MS 3000

// How long each expression keeps moving after it appears. Past that the face
// is still, so the main loop drops back to the slow frame rate (power).
#define ANIM_BOUNCE_MS 1200  // happy hop, laugh shake, heart and star pulse
#define ANIM_TREMBLE_MS 800  // angry
#define ANIM_TEAR_MS 2600    // sad: one tear runs down
#define ANIM_POP_MS 120      // surprised: eyes pop big, then settle

#define OPEN_FULL 256  // eye openness is 0..256

static const char *const NAMES[EXPR_COUNT] = {
    "neutral", "happy", "sleepy", "surprised", "angry", "love", "sad",
    "laugh",   "dizzy", "ko",     "excited",   "wink",  "lowbat",
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
    f->expr_since = now;
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
    if (e == f->expr && !f->has_pending) {
        f->expr_since = now;  // same face again: replay its animation
        return;
    }
    // Swap at the bottom of a blink: the eyes squash shut on one expression
    // and pop open on the next, which hides the change of shape.
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

static void then(face_t *f, expression_t e) {
    f->after = e;
    f->has_after = true;
}

static expression_t showing(const face_t *f) {
    return f->has_pending ? f->pending : f->expr;
}

void face_on_event(face_t *f, input_event_t ev, uint32_t now) {
    f->last_input = now;
    switch (ev) {
        case INPUT_TAP:
            // A poke: startled, then either pleased or giggling.
            react(f, EXPR_SURPRISED, now, TRANSITION_HOLD_MS);
            then(f, rng_chance(&f->rng, 50) ? EXPR_HAPPY : EXPR_LAUGH);
            break;
        case INPUT_DOUBLE_TAP:
            react(f, EXPR_LOVE, now, 4000);
            break;
        case INPUT_SHAKE:
            // Shaken while still dizzy: knocked out.
            if (showing(f) == EXPR_DIZZY) {
                react(f, EXPR_KO, now, KO_MS);
                then(f, EXPR_SAD);
            } else {
                react(f, EXPR_DIZZY, now, DIZZY_MS);
                then(f, EXPR_ANGRY);
            }
            break;
        case INPUT_TILT_LEFT:
            face_look(f, -GLANCE_X, 0, now, 2000);
            break;
        case INPUT_TILT_RIGHT:
        case INPUT_BTN_C_SHORT:
            face_look(f, GLANCE_X, 0, now, 2000);
            break;
        case INPUT_BTN_A_SHORT: {
            // A: pet it. Something cheerful, never the same twice in a row.
            static const expression_t PETTED[] = {EXPR_HAPPY, EXPR_LAUGH, EXPR_WINK, EXPR_EXCITED};
            const int n = (int)(sizeof(PETTED) / sizeof(PETTED[0]));
            expression_t e;
            do {
                e = PETTED[rng_range(&f->rng, 0, (uint32_t)n - 1)];
            } while (e == showing(f));
            react(f, e, now, e == EXPR_WINK ? 1500 : 3000);
            if (e == EXPR_WINK) then(f, EXPR_HAPPY);
            break;
        }
        case INPUT_BTN_B_SHORT: {
            // B: step through every face, the way to see them all.
            const expression_t next = (expression_t)((showing(f) + 1) % EXPR_LOWBAT);
            react(f, next, now, 8000);
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
            f->expr_since = now;
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

// How long the current expression keeps moving after it appeared (0 = still).
static uint32_t anim_ms(expression_t e) {
    switch (e) {
        case EXPR_HAPPY:
        case EXPR_LAUGH:
        case EXPR_LOVE:
        case EXPR_EXCITED: return ANIM_BOUNCE_MS;
        case EXPR_ANGRY: return ANIM_TREMBLE_MS;
        case EXPR_SAD: return ANIM_TEAR_MS;
        case EXPR_SURPRISED: return ANIM_POP_MS;
        case EXPR_DIZZY: return DIZZY_MS;  // spins for as long as it shows
        case EXPR_KO: return KO_MS;
        default: return 0;
    }
}

bool face_is_animating(const face_t *f) {
    if (f->blinking || f->has_pending || f->idle == IDLE_YAWN || f->look_x != f->look_tx ||
        f->look_y != f->look_ty) {
        return true;
    }
    return !f->night && f->now - f->expr_since < anim_ms(f->expr);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

// Sine of t/period, one full turn per period, scaled to +-amp. A 16-step
// table is plenty for a few pixels of wobble and needs no maths library.
static int wave(uint32_t t, uint32_t period, int amp) {
    static const int8_t SIN16[16] = {0, 38, 71, 92, 100, 92, 71, 38, 0, -38, -71, -92, -100, -92, -71, -38};
    return SIN16[(t * 16 / period) % 16] * amp / 100;
}

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

// Round eye squashed vertically by `open`; a flat bar once nearly shut. The
// squash also widens it a little, which is what makes the blink look bouncy.
static void draw_dot(gfx_t *g, int cx, int cy, int r, int open) {
    const int ry = r * open / OPEN_FULL;
    if (ry < 2) {
        gfx_fill_rect(g, cx - r, cy - 1, 2 * r + 1, 2, GFX_WHITE);
        return;
    }
    gfx_fill_ellipse(g, cx, cy, r + (r - ry) / 3, ry, GFX_WHITE);
}

// Thin arc of half-width w: d > 0 sags down (smile), d < 0 bows up (frown).
static void draw_arc(gfx_t *g, int cx, int cy, int w, int d) {
    for (int x = -w; x <= w; x++) {
        const int y = cy + d * (w * w - x * x) / (w * w);
        gfx_fill_rect(g, cx + x, y, 1, STROKE, GFX_WHITE);
    }
}

// Thick straight stroke, STROKE px tall.
static void stroke(gfx_t *g, int x0, int y0, int x1, int y1) {
    for (int k = 0; k < STROKE; k++) gfx_line(g, x0, y0 + k, x1, y1 + k, GFX_WHITE);
}

static void draw_caret(gfx_t *g, int cx, int cy) {  // ^
    stroke(g, cx - 9, cy + 3, cx, cy - 6);
    stroke(g, cx, cy - 6, cx + 9, cy + 3);
}

static void draw_chevron(gfx_t *g, int cx, int cy, bool right) {  // > or <
    const int s = right ? 1 : -1;
    for (int k = 0; k < STROKE; k++) {
        gfx_line(g, cx - 6 * s + k, cy - 7, cx + 6 * s + k, cy, GFX_WHITE);
        gfx_line(g, cx + 6 * s + k, cy, cx - 6 * s + k, cy + 7, GFX_WHITE);
    }
}

static void draw_x(gfx_t *g, int cx, int cy) {
    for (int k = 0; k < STROKE; k++) {
        gfx_line(g, cx - 7 + k, cy - 7, cx + 7 + k, cy + 7, GFX_WHITE);
        gfx_line(g, cx + 7 + k, cy - 7, cx - 7 + k, cy + 7, GFX_WHITE);
    }
}

static void draw_heart(gfx_t *g, int cx, int cy, int s) {
    const int r = s / 4 + 1;
    gfx_fill_circle(g, cx - s / 4, cy - s / 6, r, GFX_WHITE);
    gfx_fill_circle(g, cx + s / 4, cy - s / 6, r, GFX_WHITE);
    gfx_fill_triangle(g, cx - s / 2, cy - s / 10, cx + s / 2, cy - s / 10, cx, cy + s / 2,
                      GFX_WHITE);
}

static void draw_star(gfx_t *g, int cx, int cy, int s) {  // four-point sparkle
    gfx_fill_triangle(g, cx - s, cy, cx + s, cy, cx, cy - s - s / 2, GFX_WHITE);
    gfx_fill_triangle(g, cx - s, cy, cx + s, cy, cx, cy + s + s / 2, GFX_WHITE);
    gfx_fill_triangle(g, cx, cy - s, cx, cy + s, cx - s - s / 2, cy, GFX_WHITE);
    gfx_fill_triangle(g, cx, cy - s, cx, cy + s, cx + s + s / 2, cy, GFX_WHITE);
}

// Spiral of 2 px line segments turning by `phase` (0..63, one turn): about
// 2.3 turns growing to 11 px, the proportions of the approved concept.
static void draw_spiral(gfx_t *g, int cx, int cy, int phase) {
    static const int16_t C[64] = {256, 255, 251, 245, 237, 226, 213, 198, 181, 162, 142, 121, 98, 74, 50, 25, 0, -25, -50, -74, -98, -121, -142, -162, -181, -198, -213, -226, -237, -245, -251, -255, -256, -255, -251, -245, -237, -226, -213, -198, -181, -162, -142, -121, -98, -74, -50, -25, 0, 25, 50, 74, 98, 121, 142, 162, 181, 198, 213, 226, 237, 245, 251, 255};  // unit circle, x256
    int px = cx, py = cy;
    for (int s = 1; s <= 36; s++) {
        const int a = (phase + s * 4) % 64;  // ~0.4 rad per segment
        const int r = s * 11 / 36;
        const int x = cx + C[a] * r / 256, y = cy + C[(a + 48) % 64] * r / 256;
        gfx_line(g, px, py, x, y, GFX_WHITE);
        gfx_line(g, px + 1, py, x + 1, y, GFX_WHITE);
        px = x;
        py = y;
    }
}

static void draw_tear(gfx_t *g, int x, int y) {
    gfx_fill_triangle(g, x, y - 4, x - 2, y, x + 2, y, GFX_WHITE);
    gfx_fill_circle(g, x, y + 1, 2, GFX_WHITE);
}

// Open mouth: lower half of an ellipse (laughing, excited).
static void draw_open_mouth(gfx_t *g, int cx, int cy, int rx, int ry) {
    gfx_fill_ellipse(g, cx, cy, rx, ry, GFX_WHITE);
    gfx_fill_rect(g, cx - rx - 1, cy - ry - 1, 2 * rx + 3, ry + 1, GFX_BLACK);
}

static void draw_yawn(gfx_t *g, int cx, int cy, int amount) {
    const int ry = 1 + 5 * amount / 256;
    gfx_fill_ellipse(g, cx, cy, 4, ry, GFX_WHITE);
    if (ry >= 3) gfx_fill_ellipse(g, cx, cy, 2, ry - 2, GFX_BLACK);
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
    draw_z(g, 108 + phase / 6, 22 - phase / 2, 5);
    draw_z(g, 106 + p2 / 6, 24 - p2 / 2, 3);
}

void face_render(const face_t *f, gfx_t *g) {
    if (f->night) {
        // Asleep: two flat lines and drifting z's. Far fewer lit pixels than
        // open eyes, which is also what we want at night for power.
        gfx_fill_rect(g, EYE_L_CX - EYE_R, EYE_CY + 2, 2 * EYE_R, 2, GFX_WHITE);
        gfx_fill_rect(g, EYE_R_CX - EYE_R, EYE_CY + 2, 2 * EYE_R, 2, GFX_WHITE);
        draw_zzz(g, f->now);
        return;
    }

    const uint32_t t = f->now - f->expr_since;
    const bool moving = t < anim_ms(f->expr);
    int dx = f->look_x, dy = f->look_y;

    // Whole-face motion: happy hops, laugh and anger shake.
    if (moving && (f->expr == EXPR_HAPPY || f->expr == EXPR_EXCITED)) dy -= abs(wave(t, 600, 2));
    if (moving && (f->expr == EXPR_LAUGH || f->expr == EXPR_ANGRY)) dx += (t / 66) % 2 ? 1 : -1;

    const int lx = EYE_L_CX + dx, rx = EYE_R_CX + dx, ey = EYE_CY + dy;
    const int mx = MOUTH_CX + dx / 2, my = MOUTH_CY + dy / 2;
    const int open = openness(f);
    const int yawn = yawn_amount(f);

    switch (f->expr) {
        case EXPR_NEUTRAL:
            draw_dot(g, lx, ey, EYE_R, open);
            draw_dot(g, rx, ey, EYE_R, open);
            draw_arc(g, mx, my, 14, 5);
            break;
        case EXPR_HAPPY:
            draw_caret(g, lx, ey);
            draw_caret(g, rx, ey);
            draw_arc(g, mx, my, 15, 7);
            break;
        case EXPR_SLEEPY:
            draw_dot(g, lx, ey + 2, EYE_R, open / 3);  // heavy lids: a squashed dot
            draw_dot(g, rx, ey + 2, EYE_R, open / 3);
            gfx_fill_rect(g, mx - 3, my + 2, 6, 2, GFX_WHITE);
            draw_zzz(g, f->now);
            break;
        case EXPR_SURPRISED: {
            const int r = t < ANIM_POP_MS ? EYE_R + 4 : EYE_R + 2;
            draw_dot(g, lx, ey - 1, r, open);
            draw_dot(g, rx, ey - 1, r, open);
            gfx_fill_circle(g, mx, my + 2, 3, GFX_WHITE);
            gfx_fill_circle(g, mx, my + 2, 1, GFX_BLACK);
            break;
        }
        case EXPR_ANGRY:
            draw_dot(g, lx, ey + 2, EYE_R - 1, open);
            draw_dot(g, rx, ey + 2, EYE_R - 1, open);
            for (int k = 0; k < STROKE; k++) {  // brows slanting down toward the middle
                gfx_line(g, lx - 12, ey - 12 + k, lx + 8, ey - 5 + k, GFX_WHITE);
                gfx_line(g, rx + 12, ey - 12 + k, rx - 8, ey - 5 + k, GFX_WHITE);
            }
            draw_arc(g, mx, my + 6, 11, -4);
            break;
        case EXPR_LOVE: {
            const int s = 21 + (moving ? wave(t, 500, 2) : 0);
            draw_heart(g, lx, ey, s);
            draw_heart(g, rx, ey, s);
            draw_arc(g, mx, my, 14, 5);
            break;
        }
        case EXPR_SAD:
            draw_dot(g, lx - 2, ey + 3, EYE_R - 1, open);
            draw_dot(g, rx - 2, ey + 3, EYE_R - 1, open);
            draw_arc(g, mx - 2, my + 7, 11, -5);
            if (moving) draw_tear(g, lx - 8, ey + 15 + (int)(t * 12 / ANIM_TEAR_MS));
            break;
        case EXPR_LAUGH:
            draw_chevron(g, lx, ey, true);
            draw_chevron(g, rx, ey, false);
            draw_open_mouth(g, mx, my + 1, 6, 4 + (moving && (t / 100) % 2));
            break;
        case EXPR_DIZZY: {
            const int phase = (int)(t / 16 % 64);
            draw_spiral(g, lx, ey, phase);
            draw_spiral(g, rx, ey, (phase + 32) % 64);
            for (int x = -8; x <= 8; x++) gfx_fill_rect(g, mx + x, my + 1 + wave(t + (uint32_t)x * 40, 600, 1), 1, 2, GFX_WHITE);
            break;
        }
        case EXPR_KO: {
            draw_x(g, lx, ey);
            draw_x(g, rx, ey);
            gfx_fill_rect(g, mx - 5, my + 1, 11, 2, GFX_WHITE);
            // A star circling overhead.
            draw_star(g, MOUTH_CX + dx + wave(t, 1200, 28), 11 + dy + wave(t + 300, 1200, 2), 2);
            break;
        }
        case EXPR_EXCITED: {
            const int s = 7 + (moving ? wave(t, 400, 2) : 0);
            draw_star(g, lx, ey, s);
            draw_star(g, rx, ey, s);
            draw_open_mouth(g, mx, my + 1, 6, 5);
            break;
        }
        case EXPR_WINK:
            draw_dot(g, lx, ey, EYE_R, open);
            draw_caret(g, rx, ey);
            draw_arc(g, mx, my, 15, 6);
            break;
        case EXPR_LOWBAT:
            draw_dot(g, lx, ey + 3, EYE_R, open / 4);
            draw_dot(g, rx, ey + 3, EYE_R, open / 4);
            // Empty battery for a mouth, outline blinking at 1 Hz.
            if ((f->now / 500) % 2 == 0) {
                gfx_rect(g, mx - 8, my, 15, 9, GFX_WHITE);
                gfx_fill_rect(g, mx + 7, my + 2, 2, 5, GFX_WHITE);
            }
            break;
        case EXPR_COUNT:
            break;
    }

    // A yawn opens the mouth over whatever the expression has there.
    if (yawn) {
        gfx_fill_rect(g, mx - 13, my - 1, 27, 9, GFX_BLACK);
        draw_yawn(g, mx, my + 3, yawn);
    }
}
