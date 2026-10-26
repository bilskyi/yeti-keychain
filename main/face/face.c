#include "face/face.h"

#include <stdlib.h>

#include "config.h"
#include "display/font.h"

// ---------------------------------------------------------------------------
// Geometry: the EMO look (Sasha's pick, 2026-10-26, concept 4 "Pill Eyes").
// No mouth: two tall rounded eyes, and every feeling is in their shape: cut
// from below they smile, a slanted cut from above is anger or sadness,
// squashed is sleepy. A small mouth shows only to yawn or sneeze. The face is
// drawn for a centred screen; the burn-in shift moves it by up to
// BURNIN_MAX_SHIFT_PX through the gfx offset, and everything below keeps a
// margin of at least that plus the largest glance.
// ---------------------------------------------------------------------------
#define EYE_L_CX 36
#define EYE_R_CX 92
#define EYE_CY 31
#define EYE_W 32
#define EYE_H 42
#define EYE_RAD 11
#define MOUTH_CX 64
#define MOUTH_CY 50  // yawns and sneezes only
#define STROKE 3     // line weight of the X eyes

#define GLANCE_X 12  // how far a glance moves the eyes sideways
#define GLANCE_Y 3

#define YAWN_MS 2600
#define SNEEZE_MS 1400
#define SNEEZE_AT_MS 700  // the "choo"
#define SNEEZE_CHOO_MS 250
#define HUM_MS 2600
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

// Body sense (face_feel).
#define GAZE_DEAD_MG 150   // tilt below this: the eyes do their own thing
#define GAZE_FULL_MG 450   // this much tilt looks all the way over
#define GAZE_STEP_PX 3     // retarget only on a change this big (sensor noise)
#define GAZE_HOLD_MS 800
#define FLIP_ENTER_MG (-650)  // "up" axis reading below this: upside down
#define FLIP_EXIT_MG 300
#define FLIP_HOLD_MS 600
#define SCARED_HOLD_MS 1500
#define STILL_DELTA_MG 60     // sample-to-sample change below this: at rest
#define PICKUP_DELTA_MG 250   // ...and above this after a long rest: picked up
#define PICKUP_REST_MS 90000
#define FALL_MG 350           // |a| below this: falling (weightless)
#define WHEE_COOLDOWN_MS 3000

// Thought bubble: shown for BUBBLE_ON_S out of every BUBBLE_EVERY_S seconds
// while the pet needs something.
#define BUBBLE_EVERY_S 15
#define BUBBLE_ON_S 4

static const char *const NAMES[EXPR_COUNT] = {
    "neutral", "happy", "sleepy", "surprised", "angry", "love", "sad",
    "laugh",   "dizzy", "ko",     "excited",   "wink",  "scared", "lowbat",
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

void face_react(face_t *f, expression_t e, uint32_t now, uint32_t hold_ms) {
    f->last_input = now;
    react(f, e, now, hold_ms);
}

static void then(face_t *f, expression_t e) {
    f->after = e;
    f->has_after = true;
}

static expression_t showing(const face_t *f) {
    return f->has_pending ? f->pending : f->expr;
}

void face_on_event(face_t *f, input_event_t ev, uint32_t now) {
    if (ev >= INPUT_BTN_A_PRESS && ev <= INPUT_BTN_C_PRESS) return;  // SHORT/LONG follow
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
        case INPUT_BTN_A_SHORT:
        case INPUT_BTN_A_LONG: {
            // A: pet it (a long press too: no hidden meanings for A). Something cheerful, never the same twice in a row.
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
        case INPUT_BTN_B_LONG: {
            // B held: step through every face (hidden; B opens the menu).
            const expression_t next = (expression_t)((showing(f) + 1) % EXPR_LOWBAT);
            react(f, next, now, 8000);
            break;
        }
        case INPUT_BTN_C_LONG:
            react(f, EXPR_HAPPY, now, 3000);
            break;
        case INPUT_BTN_B_SHORT:  // opens the menu (main.c)
        case INPUT_BTN_A_PRESS:
        case INPUT_BTN_B_PRESS:
        case INPUT_BTN_C_PRESS:
        case INPUT_NONE:
        case INPUT_EVENT_COUNT:
            break;
    }
}

void face_set_needs(face_t *f, uint8_t needs) {
    f->needs = needs;
}

void face_fx(face_t *f, face_fx_t fx, const char *text, uint32_t now, uint32_t ms) {
    f->fx = fx;
    f->fx_since = now;
    f->fx_until = ms ? now + ms : 0;
    f->fx_text = text;
    f->idle = IDLE_NONE;
    f->last_input = now;
    // The expression underneath, and what follows.
    switch (fx) {
        case FX_ALARM:
            react(f, EXPR_SAD, now, ms + 2000);
            break;
        case FX_GLITCH:
            react(f, EXPR_DIZZY, now, ms);
            then(f, EXPR_SURPRISED);  // "...what was that?"
            break;
        case FX_SING:
            react(f, EXPR_HAPPY, now, ms ? ms : UINT32_MAX / 4);
            break;
        case FX_NONE:
            break;
    }
}

void face_fx_stop(face_t *f) {
    if (f->fx == FX_SING) f->expr_until = f->now;  // let the mood machine back in
    f->fx = FX_NONE;
}

face_cue_t face_take_cue(face_t *f) {
    const face_cue_t c = f->cue;
    f->cue = CUE_NONE;
    return c;
}

// Upside down, with hysteresis and a hold so a quick flip of the wrist is not
// a panic. Returns true while upside down.
static bool feel_flip(face_t *f, int up, uint32_t now) {
    const bool want = f->upside ? up < FLIP_EXIT_MG : up < FLIP_ENTER_MG;
    if (want == f->upside) {
        f->flip_pending = false;
    } else if (!f->flip_pending) {
        f->flip_pending = true;
        f->flip_since = now;
    } else if (now - f->flip_since >= FLIP_HOLD_MS) {
        f->flip_pending = false;
        f->upside = want;
        f->last_input = now;
        if (want) {
            react(f, EXPR_SCARED, now, SCARED_HOLD_MS);
            f->cue = CUE_SCARED;
        } else {
            react(f, EXPR_HAPPY, now, 2500);  // phew
            f->cue = CUE_RELIEF;
        }
    }
    if (f->upside) {
        // Keep panicking for as long as it stays that way round, whatever
        // else happens in between.
        if (showing(f) == EXPR_SCARED) {
            f->expr_until = now + SCARED_HOLD_MS;
        } else if (reached(now, f->expr_until)) {
            react(f, EXPR_SCARED, now, SCARED_HOLD_MS);
        }
    }
    return f->upside;
}

void face_feel(face_t *f, const int mg[3], bool oriented, uint32_t now) {
    if (f->night) {  // asleep: feels nothing
        f->has_prev = false;
        f->upside = f->flip_pending = false;
        return;
    }
    int delta = 0;
    for (int i = 0; i < 3; i++) {
        if (f->has_prev) delta += abs(mg[i] - f->prev_mg[i]);
        f->prev_mg[i] = mg[i];
    }
    if (!f->has_prev) {
        f->has_prev = true;
        f->still_since = now;
    }

    // Weightless for two samples in a row: falling, or thrown up.
    const int m2 = mg[0] * mg[0] + mg[1] * mg[1] + mg[2] * mg[2];
    if (m2 < FALL_MG * FALL_MG) {
        if (++f->low_g == 2 && reached(now, f->whee_until)) {
            f->last_input = now;
            react(f, EXPR_EXCITED, now, 1500);
            then(f, EXPR_HAPPY);
            f->cue = CUE_WHEE;
            f->whee_until = now + WHEE_COOLDOWN_MS;
        }
    } else {
        f->low_g = 0;
    }

    // Picked up after lying still a long while: hello!
    if (delta > PICKUP_DELTA_MG && now - f->still_since >= PICKUP_REST_MS && !f->upside) {
        f->last_input = now;
        react(f, EXPR_SURPRISED, now, TRANSITION_HOLD_MS);
        then(f, EXPR_HAPPY);
        f->cue = CUE_HI;
    }
    if (delta > STILL_DELTA_MG) f->still_since = now;

    // Which way is up means nothing until the sensor is calibrated.
    if (oriented && feel_flip(f, mg[1], now)) return;

    // The eyes roll toward the low side, like marbles.
    int tilt = ACCEL_TILT_SIGN * mg[ACCEL_TILT_AXIS];
    if (abs(tilt) < GAZE_DEAD_MG) return;
    if (tilt > GAZE_FULL_MG) tilt = GAZE_FULL_MG;
    if (tilt < -GAZE_FULL_MG) tilt = -GAZE_FULL_MG;
    const int tx = tilt * GLANCE_X / GAZE_FULL_MG;
    if (abs(tx - f->look_tx) >= GAZE_STEP_PX || !f->look_until) {
        face_look(f, tx, 0, now, GAZE_HOLD_MS);
    } else {
        f->look_until = now + GAZE_HOLD_MS;
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
    const bool yawn = f->expr == EXPR_SLEEPY || (f->needs & PET_NEED_SLEEP)
                          ? rng_chance(&f->rng, 50)
                          : rng_chance(&f->rng, 12);
    if (yawn) {
        f->idle = IDLE_YAWN;
        f->idle_start = now;
        return;
    }
    if (rng_chance(&f->rng, 7)) {
        f->idle = IDLE_SNEEZE;
        f->idle_start = now;
        f->cue = CUE_SNEEZE;
        return;
    }
    const bool cheerful = f->expr == EXPR_HAPPY || f->expr == EXPR_NEUTRAL || f->expr == EXPR_LOVE;
    if (cheerful && !f->needs && rng_chance(&f->rng, 15)) {
        f->idle = IDLE_HUM;
        f->idle_start = now;
        f->cue = CUE_HUM;
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
                .needs = f->needs,
            };
            uint32_t hold;
            const expression_t next = mood_pick(&f->rng, f->expr, &ctx, &hold);
            face_set_expression(f, next, now, hold);
        }
    }

    if (f->fx != FX_NONE && f->fx_until && reached(now, f->fx_until)) f->fx = FX_NONE;

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
        case IDLE_SNEEZE:
        case IDLE_HUM: {
            const uint32_t len = f->idle == IDLE_YAWN ? YAWN_MS : f->idle == IDLE_SNEEZE ? SNEEZE_MS : HUM_MS;
            if (now - f->idle_start >= len) {
                f->idle = IDLE_NONE;
                schedule_idle(f, now);
            }
            break;
        }
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
        case EXPR_SCARED: return UINT32_MAX;  // trembles for as long as it shows
        default: return 0;
    }
}

bool face_is_animating(const face_t *f) {
    if (f->fx != FX_NONE) return true;
    if (f->blinking || f->has_pending || (f->idle != IDLE_NONE && f->idle != IDLE_GLANCE) ||
        f->look_x != f->look_tx ||
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
    // So does the "ah... ah..." before a sneeze.
    if (f->idle == IDLE_SNEEZE) {
        const int t = (int)(f->now - f->idle_start);
        if (t < SNEEZE_AT_MS) open = open * (256 - t * 180 / SNEEZE_AT_MS) / 256;
    }
    return open;
}

// The EMO eye: a tall rounded rectangle, squashed vertically by `open` down to
// a flat bar. The squash also widens it a little, which makes blinks bouncy.
static void draw_pill(gfx_t *g, int cx, int cy, int w, int h, int open) {
    int hh = h * open / OPEN_FULL;
    if (hh < 3) hh = 3;
    const int ww = w + (h - hh) / 5;  // squashing widens it a little: bouncy
    gfx_fill_rrect(g, cx - ww / 2, cy - hh / 2, ww, hh, EYE_RAD, GFX_WHITE);
}

// Smiling eye: a pill with its lower part bitten away by a black ellipse,
// leaving an arch.
static void draw_smile_eye(gfx_t *g, int cx, int cy) {
    const int w = EYE_W + 2;
    gfx_fill_rrect(g, cx - w / 2, cy - EYE_H / 2, w, EYE_H, EYE_RAD, GFX_WHITE);
    gfx_fill_ellipse(g, cx, cy + EYE_H * 16 / 30, w * 17 / 26, EYE_H * 13 / 30, GFX_BLACK);
}

// A pill with one top corner cut on a slant. inner = cut the corner nearest
// the middle (angry); otherwise the outer one (sad). side: -1 left eye, +1 right.
static void draw_cut_eye(gfx_t *g, int cx, int cy, int w, int h, int open, int side, bool inner) {
    draw_pill(g, cx, cy, w, h, open);
    const int top = cy - h / 2 - 2, l = cx - w / 2 - 3, r = cx + w / 2 + 3;
    const bool cut_right = inner ? side < 0 : side > 0;
    if (cut_right) {
        gfx_fill_triangle(g, l, top, r, top, r, cy + 1, GFX_BLACK);
    } else {
        gfx_fill_triangle(g, l, top, r, top, l, cy + 1, GFX_BLACK);
    }
}

static void draw_x(gfx_t *g, int cx, int cy) {
    for (int k = 0; k < STROKE; k++) {
        gfx_line(g, cx - 13 + k, cy - 13, cx + 13 + k, cy + 13, GFX_WHITE);
        gfx_line(g, cx + 13 + k, cy - 13, cx - 13 + k, cy + 13, GFX_WHITE);
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
// 2.3 turns growing to 15 px.
static void draw_spiral(gfx_t *g, int cx, int cy, int phase) {
    static const int16_t C[64] = {256, 255, 251, 245, 237, 226, 213, 198, 181, 162, 142, 121, 98, 74, 50, 25, 0, -25, -50, -74, -98, -121, -142, -162, -181, -198, -213, -226, -237, -245, -251, -255, -256, -255, -251, -245, -237, -226, -213, -198, -181, -162, -142, -121, -98, -74, -50, -25, 0, 25, 50, 74, 98, 121, 142, 162, 181, 198, 213, 226, 237, 245, 251, 255};  // unit circle, x256
    int px = cx, py = cy;
    for (int s = 1; s <= 36; s++) {
        const int a = (phase + s * 4) % 64;  // ~0.4 rad per segment
        const int r = s * 15 / 36;
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

static void draw_note(gfx_t *g, int x, int y) {  // a quaver
    gfx_fill_ellipse(g, x, y, 2, 2, GFX_WHITE);
    gfx_fill_rect(g, x + 2, y - 7, 1, 8, GFX_WHITE);
    gfx_line(g, x + 2, y - 7, x + 5, y - 4, GFX_WHITE);
}

// Thought bubble in the top right corner with what the pet wants. It sits
// over the corner of the right eye, so it gets a black outline first.
static void draw_bubble(gfx_t *g, uint8_t needs) {
    gfx_fill_circle(g, 104, 19, 3, GFX_BLACK);
    gfx_fill_circle(g, 108, 15, 4, GFX_BLACK);
    gfx_fill_rrect(g, 106, 0, 22, 17, 6, GFX_BLACK);
    gfx_fill_circle(g, 104, 19, 1, GFX_WHITE);
    gfx_fill_circle(g, 108, 15, 2, GFX_WHITE);
    gfx_fill_rrect(g, 108, 2, 18, 13, 5, GFX_WHITE);
    const int cx = 117, cy = 8;
    if (needs & PET_NEED_FOOD) {  // a fish
        gfx_fill_ellipse(g, cx - 2, cy, 4, 3, GFX_BLACK);
        gfx_fill_triangle(g, cx + 1, cy, cx + 5, cy - 3, cx + 5, cy + 3, GFX_BLACK);
    } else if (needs & PET_NEED_SLEEP) {  // a z
        gfx_hline(g, cx - 3, cx + 3, cy - 3, GFX_BLACK);
        gfx_line(g, cx + 3, cy - 3, cx - 3, cy + 3, GFX_BLACK);
        gfx_hline(g, cx - 3, cx + 3, cy + 3, GFX_BLACK);
    } else {  // a ball: wants to play
        gfx_fill_circle(g, cx, cy, 4, GFX_BLACK);
        gfx_hline(g, cx - 4, cx + 4, cy, GFX_WHITE);
    }
}

// Tears the finished picture: a few bands slide sideways, some pixels flip.
// Driven by a hash of the time so rendering stays a pure function.
static void glitch_post(gfx_t *g, uint32_t t) {
    uint32_t h = (t / 60 + 1) * 2654435761u;
    uint8_t row[GFX_W];
    for (int b = 0; b < 3; b++) {
        h ^= h << 13;
        h ^= h >> 17;
        h ^= h << 5;
        const int y0 = (int)(h % GFX_H), hgt = 2 + (int)(h >> 8) % 6, dx = (int)(h >> 16) % 25 - 12;
        for (int y = y0; y < y0 + hgt && y < GFX_H; y++) {
            for (int x = 0; x < GFX_W; x++) row[x] = (uint8_t)gfx_get_pixel(g, x, y);
            for (int x = 0; x < GFX_W; x++) {
                const int sx = ((x - dx) % GFX_W + GFX_W) % GFX_W;
                gfx_pixel(g, x - g->ox, y - g->oy, row[sx] ? GFX_WHITE : GFX_BLACK);
            }
        }
    }
    for (int k = 0; k < 40; k++) {  // static
        h = h * 1103515245u + 12345u;
        const int x = (int)(h >> 8) % GFX_W, y = (int)(h >> 20) % GFX_H;
        gfx_pixel(g, x - g->ox, y - g->oy, gfx_get_pixel(g, x, y) ? GFX_BLACK : GFX_WHITE);
    }
}

static void invert(gfx_t *g) {
    for (int i = 0; i < GFX_FB_BYTES; i++) g->buf[i] = (uint8_t)~g->buf[i];
}

// Warning sign, top right: a triangle with a "!".
static void draw_warning(gfx_t *g) {
    gfx_fill_triangle(g, 114, 0, 100, 24, 128, 24, GFX_BLACK);  // outline over the eye
    gfx_fill_triangle(g, 114, 3, 103, 22, 125, 22, GFX_WHITE);
    gfx_fill_rect(g, 113, 9, 3, 7, GFX_BLACK);
    gfx_fill_rect(g, 113, 18, 3, 2, GFX_BLACK);
}

// Singing: eyes happily shut, mouth opening on the beat, bobbing, notes.
static void draw_singing(const face_t *f, gfx_t *g) {
    const uint32_t t = f->now - f->fx_since;
    const int dy = -abs(wave(t, 700, 2)), dx = wave(t, 1400, 3);
    draw_smile_eye(g, EYE_L_CX + dx, EYE_CY + dy);
    draw_smile_eye(g, EYE_R_CX + dx, EYE_CY + dy);
    // Two notes rise in the gap between the eyes, half a cycle apart.
    static const int8_t NOTE_X[2] = {59, 66};
    for (int k = 0; k < 2; k++) {
        const int q = (int)(t / 45 + (uint32_t)k * 11) % 22;  // the stem stays off the top edge
        draw_note(g, NOTE_X[k], 34 - q);
    }
}

void face_render(const face_t *f, gfx_t *g) {
    if (f->night) {
        // Asleep: two flat lines and drifting z's. Far fewer lit pixels than
        // open eyes, which is also what we want at night for power.
        gfx_fill_rrect(g, EYE_L_CX - EYE_W / 2, EYE_CY + EYE_H / 4, EYE_W, 4, 2, GFX_WHITE);
        gfx_fill_rrect(g, EYE_R_CX - EYE_W / 2, EYE_CY + EYE_H / 4, EYE_W, 4, 2, GFX_WHITE);
        draw_zzz(g, f->now);
        return;
    }

    if (f->fx == FX_SING) {
        draw_singing(f, g);
        return;
    }

    const uint32_t t = f->now - f->expr_since;
    const bool moving = t < anim_ms(f->expr);
    int dx = f->look_x, dy = f->look_y;

    // Whole-face motion: happy hops, laugh and anger shake.
    if (moving && (f->expr == EXPR_HAPPY || f->expr == EXPR_EXCITED)) dy -= abs(wave(t, 600, 2));
    if (moving && f->expr == EXPR_LAUGH) dy -= abs(wave(t, 300, 3));  // giggling hops
    if (moving && (f->expr == EXPR_LAUGH || f->expr == EXPR_ANGRY)) dx += (t / 66) % 2 ? 1 : -1;

    const int lx = EYE_L_CX + dx, rx = EYE_R_CX + dx, ey = EYE_CY + dy;
    const int mx = MOUTH_CX + dx, my = MOUTH_CY + dy;
    const int open = openness(f);
    const int yawn = yawn_amount(f);

    // The "choo!" of a sneeze: eyes squeezed shut to bars, a jolt down.
    if (f->idle == IDLE_SNEEZE) {
        const uint32_t st = f->now - f->idle_start;
        if (st >= SNEEZE_AT_MS && st < SNEEZE_AT_MS + SNEEZE_CHOO_MS) {
            gfx_fill_rrect(g, lx - EYE_W / 2, ey + 3, EYE_W, 5, 2, GFX_WHITE);
            gfx_fill_rrect(g, rx - EYE_W / 2, ey + 3, EYE_W, 5, 2, GFX_WHITE);
            draw_open_mouth(g, mx, my, 5, 4);
            for (int k = 0; k < 3; k++) gfx_pixel(g, mx - 4 + k * 4, my + 7 + k % 2, GFX_WHITE);
            return;
        }
    }

    switch (f->expr) {
        case EXPR_NEUTRAL:
            draw_pill(g, lx, ey, EYE_W, EYE_H, open);
            draw_pill(g, rx, ey, EYE_W, EYE_H, open);
            break;
        case EXPR_HAPPY:
            draw_smile_eye(g, lx, ey);
            draw_smile_eye(g, rx, ey);
            break;
        case EXPR_SLEEPY:
            // Heavy lids: only the bottom of each eye shows.
            for (int s = 0; s < 2; s++) {
                const int cx = s ? rx : lx;
                draw_pill(g, cx, ey + 2, EYE_W, EYE_H - 4, OPEN_FULL);
                const int lid = (EYE_H - 4) * 5 / 8 + (OPEN_FULL - open) * 8 / OPEN_FULL;
                gfx_fill_rect(g, cx - EYE_W / 2 - 4, ey + 2 - (EYE_H - 4) / 2 - 1, EYE_W + 8, lid + 1, GFX_BLACK);
            }
            draw_zzz(g, f->now);
            break;
        case EXPR_SURPRISED: {
            const int grow = t < ANIM_POP_MS ? 8 : 5;
            draw_pill(g, lx, ey - 1, EYE_W + grow / 2, EYE_H + grow, open);
            draw_pill(g, rx, ey - 1, EYE_W + grow / 2, EYE_H + grow, open);
            break;
        }
        case EXPR_ANGRY:
            draw_cut_eye(g, lx, ey + 2, EYE_W, EYE_H - 4, open, -1, true);
            draw_cut_eye(g, rx, ey + 2, EYE_W, EYE_H - 4, open, 1, true);
            break;
        case EXPR_LOVE: {
            const int s = 34 + (moving ? wave(t, 500, 3) : 0);
            draw_heart(g, lx, ey, s);
            draw_heart(g, rx, ey, s);
            break;
        }
        case EXPR_SAD:
            draw_cut_eye(g, lx, ey + 4, EYE_W - 2, EYE_H - 6, open, -1, false);
            draw_cut_eye(g, rx, ey + 4, EYE_W - 2, EYE_H - 6, open, 1, false);
            if (moving) draw_tear(g, lx - 8, ey + EYE_H / 2 + 4 + (int)(t * 10 / ANIM_TEAR_MS));
            break;
        case EXPR_LAUGH:
            draw_smile_eye(g, lx, ey);
            draw_smile_eye(g, rx, ey);
            break;
        case EXPR_DIZZY: {
            const int phase = (int)(t / 16 % 64);
            draw_spiral(g, lx, ey, phase);
            draw_spiral(g, rx, ey, (phase + 32) % 64);
            break;
        }
        case EXPR_KO: {
            draw_x(g, lx, ey);
            draw_x(g, rx, ey);
            // A star circling overhead.
            draw_star(g, MOUTH_CX + dx + wave(t, 1200, 28), 9 + dy + wave(t + 300, 1200, 2), 2);
            break;
        }
        case EXPR_EXCITED: {
            const int s = 12 + (moving ? wave(t, 400, 2) : 0);
            draw_star(g, lx, ey, s);
            draw_star(g, rx, ey, s);
            break;
        }
        case EXPR_WINK:
            draw_pill(g, lx, ey, EYE_W, EYE_H, open);
            draw_smile_eye(g, rx, ey);
            break;
        case EXPR_SCARED: {
            // Shrunk to small trembling pills, a "!" between them, sweat.
            const int j = (t / 50) % 2 ? 1 : -1;
            draw_pill(g, lx + j, ey, 18, 24, open);
            draw_pill(g, rx + j, ey, 18, 24, open);
            gfx_fill_rect(g, 63 + dx, ey - 10, 3, 11, GFX_WHITE);
            gfx_fill_rect(g, 63 + dx, ey + 4, 3, 3, GFX_WHITE);
            draw_tear(g, rx + 17, ey - 12 + (int)(t / 120 % 6));
            break;
        }
        case EXPR_LOWBAT:
            draw_pill(g, lx, ey + 8, EYE_W, 10, open);
            draw_pill(g, rx, ey + 8, EYE_W, 10, open);
            // An empty battery between them, outline blinking at 1 Hz.
            if ((f->now / 500) % 2 == 0) {
                gfx_rect(g, mx - 8, ey + 2, 15, 9, GFX_WHITE);
                gfx_fill_rect(g, mx + 7, ey + 4, 2, 5, GFX_WHITE);
            }
            break;
        case EXPR_COUNT:
            break;
    }

    // A yawn: the only time it has a mouth.
    if (yawn) draw_yawn(g, mx, my, yawn);

    // Humming: two notes drifting up between the eyes.
    if (f->idle == IDLE_HUM) {
        const int p = (int)((f->now - f->idle_start) / 50);
        for (int k = 0; k < 2; k++) {
            const int q = (p + k * 10) % 20;
            draw_note(g, 58 + q / 3 + k * 6, 30 - q);
        }
    }

    // What it needs, now and then, while it is not busy reacting.
    if (f->needs && !f->reacting && f->expr != EXPR_SLEEPY && f->idle == IDLE_NONE &&
        (f->now / 1000) % BUBBLE_EVERY_S < BUBBLE_ON_S) {
        draw_bubble(g, f->needs);
    }

    const uint32_t ft = f->now - f->fx_since;
    if (f->fx == FX_ALARM) {
        // Siren: the whole screen flashes for a second, then the sign blinks.
        if ((ft / 250) % 2 == 0) draw_warning(g);
        if (ft < 1200 && (ft / 150) % 2) invert(g);
    } else if (f->fx == FX_GLITCH) {
        glitch_post(g, ft);
        // Halfway through, a line of its log flickers up.
        if (f->fx_text && ft > 500 && (ft / 90) % 4) {
            const int w = font_width(f->fx_text, 1);
            gfx_fill_rect(g, 64 - w / 2 - 3, 25, w + 6, 11, GFX_BLACK);
            gfx_rect(g, 64 - w / 2 - 3, 25, w + 6, 11, GFX_WHITE);
            font_draw_centered(g, 64, 27, f->fx_text, 1, GFX_WHITE);
        }
    }
}
