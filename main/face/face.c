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
#define PETTING_MAX_MS 60000  // hearts while held (TOUCH_END ends it sooner)
#define LISTEN_MAX_MS 6000    // listening face, unless TALK comes first

// Shaking: every shake within RAGE_COOL_MS of the last makes it angrier.
// 1: dizzy, then cross. 2: grumbling. 3-4: furious. RAGE_KO: knocked out,
// and then it sulks (looking away) until patted SULK_PATS times or cuddled.
#define RAGE_COOL_MS 5000
#define RAGE_KO 5
#define FURY_MS 4500
#define FURY_FLASH_MS 400  // the screen flashes as it boils over
#define SULK_MS 20000
#define SULK_PATS 3

// Petting, timed from TOUCH_HOLD (0.6 s into the touch). The TTP223 lets go
// by itself ~7 s into a touch, ~6.4 s after HOLD: in the heart overload, so
// that letting go, by hand or by the chip, plays the finale. A pad that
// holds on longer gets the finale anyway at PET_FINALE_MS.
#define PET_RUB_MS 1500     // squished under the hand, then nuzzling into it
#define PET_MELT_MS 3200    // melting: the arches sag and drip
#define PET_LOVE_MS 4800    // heart overload: beating hearts, sparkles
#define PET_FINALE_MS 7500
#define HEARTS_MS 1800      // the finale's burst

#define BOOT_MS 1600
#define FLY_MS 5600
#define FLY_LAND_AT 3300   // the fly glides in to land on the right eye...
#define FLY_SIT_AT 3700    // ...sits there...
#define FLY_SHOO_AT 4500   // ...gets blinked at...
#define FLY_OFF_AT 4600    // ...and takes off
#define SNOW_MS 6000
#define SNOW_FLAKES 14
#define BLOWN_MS 1300
#define STRETCH_MS 2200
#define ROLL_MS 1800
#define GIGGLE_AGAIN_MS 4000  // tickled again within this of a laugh: it rolls
#define ROLL_AGAIN_MS 8000    // ...unless it rolled this recently: it laughs on
#define LAUGH_ON_MS 2500      // each tap while laughing keeps it laughing this long
#define TALK_SYLLABLE_MS 150  // about one of sound_babble's chirps

#define JOLT_MS 250        // hung: how long each shake's jolt shows
#define HANG_HINT_MS 6000  // hung this long: "SHAKE ME!"

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
    "laugh",   "dizzy", "ko",     "excited",   "wink",  "scared", "furious",
    "bliss",   "lowbat",
};

const char *face_expression_name(expression_t e) {
    return (unsigned)e < EXPR_COUNT ? NAMES[e] : "?";
}

static const int8_t SIN16[16] = {0, 38, 71, 92, 100, 92, 71, 38, 0, -38, -71, -92, -100, -92, -71, -38};

// Smooth sine: SIN16 with linear interpolation, for things that glide.
static int swave(uint32_t t, uint32_t period, int amp) {
    const uint32_t ph = (t % period) * 4096 / period;
    const int i = (int)(ph >> 8), fr = (int)(ph & 255);
    const int a = SIN16[i], b = SIN16[(i + 1) % 16];
    return (a * 256 + (b - a) * fr) * amp / 25600;
}

// Where the fly is, t ms into IDLE_FLY: in from the right, loops about,
// glides down onto the right eye, sits, and shoots off up to the left.
static void fly_pos(uint32_t t, int *x, int *y) {
    const int land_x = EYE_R_CX + 5, land_y = EYE_CY - EYE_H / 2 - 4;
    if (t < FLY_LAND_AT) {
        *x = 64 + swave(t + 575, 2300, 50);
        *y = 20 + swave(t, 1100, 12);
        if (t < 500) *x += (int)(500 - t) / 6;
    } else if (t < FLY_SIT_AT) {
        int x0, y0;
        fly_pos(FLY_LAND_AT - 1, &x0, &y0);
        const int u = (int)(t - FLY_LAND_AT), n = FLY_SIT_AT - FLY_LAND_AT;
        *x = x0 + (land_x - x0) * u / n;
        *y = y0 + (land_y - y0) * u / n;
    } else if (t < FLY_OFF_AT) {
        *x = land_x;
        *y = land_y;
    } else {
        const int u = (int)(t - FLY_OFF_AT);
        *x = land_x - u / 6;
        *y = land_y - u / 40 + swave(t, 300, 2);
    }
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

static void then(face_t *f, expression_t e);

void face_set_night(face_t *f, bool night, uint32_t now) {
    if (night == f->night) return;
    f->night = night;
    f->idle = IDLE_NONE;
    face_look(f, 0, 0, now, 0);
    if (night) {
        f->reacting = false;
        face_set_expression(f, EXPR_SLEEPY, now, MOOD_MAX_MS);
    } else {
        // Woken up: a big stretch, then the mood machine.
        f->reacting = true;
        face_set_expression(f, EXPR_NEUTRAL, now, STRETCH_MS + 300);
        then(f, EXPR_HAPPY);
    }
    schedule_blink(f, now);
    schedule_idle(f, now);
    if (!night) {
        f->idle = IDLE_STRETCH;
        f->idle_start = now;
        f->cue = CUE_STRETCH;
    }
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

static void then_for(face_t *f, expression_t e, uint32_t hold_ms) {
    f->after = e;
    f->after_ms = hold_ms;
    f->has_after = true;
}

static void then(face_t *f, expression_t e) {
    then_for(f, e, 4000);
}

static bool sulking(const face_t *f, uint32_t now) {
    return f->sulk_dir != 0 && !reached(now, f->sulk_until);
}

static void reboot(face_t *f, uint32_t now) {
    f->fx = FX_REBOOT;
    f->fx_since = now;
    f->fx_until = now + FACE_REBOOT_MS;
    f->cue = CUE_REBOOT;
}

// Hung: shaking is the only thing that helps; everything else is an error.
static void hung_event(face_t *f, input_event_t ev, uint32_t now) {
    if (f->fx == FX_REBOOT) return;  // busy coming back
    switch (ev) {
        case INPUT_SHAKE:
            f->jolt_at = now;
            if (++f->hang_fixes >= FACE_HANG_FIXES) {
                reboot(f, now);
            } else {
                f->cue = CUE_JOLT;
            }
            break;
        case INPUT_TAP:
        case INPUT_DOUBLE_TAP:
        case INPUT_BTN_A_SHORT:
        case INPUT_BTN_A_LONG:
        case INPUT_BTN_B_SHORT:
        case INPUT_BTN_B_LONG:
        case INPUT_TOUCH:
        case INPUT_TOUCH_HOLD:
        case INPUT_TICKLE:
            f->cue = CUE_ERROR;
            break;
        default:  // tilt, sounds, letting go: nothing gets through
            break;
    }
}

// Shaken: angrier each time it comes soon after the last one.
static void shaken(face_t *f, uint32_t now) {
    const uint32_t calm = now - f->rage_at;
    if (calm >= (uint32_t)RAGE_COOL_MS * RAGE_KO) {
        f->rage = 0;
    } else {
        f->rage -= (int)(calm / RAGE_COOL_MS);
        if (f->rage < 0) f->rage = 0;
    }
    f->rage++;
    f->rage_at = now;
    f->petting = false;
    if (f->rage == 1) {
        react(f, EXPR_DIZZY, now, DIZZY_MS);
        then(f, EXPR_ANGRY);
    } else if (f->rage == 2) {
        react(f, EXPR_DIZZY, now, DIZZY_MS * 3 / 5);
        then(f, EXPR_ANGRY);
        f->cue = CUE_GRUMBLE;
    } else if (f->rage < RAGE_KO) {
        react(f, EXPR_FURIOUS, now, FURY_MS);
        then(f, EXPR_ANGRY);
        f->cue = CUE_FURY;
    } else {
        // Knocked out; when it comes to, it sulks (face_update).
        react(f, EXPR_KO, now, KO_MS);
        f->rage = 0;
        f->sulk_until = now + KO_MS + SULK_MS;
        f->sulk_dir = rng_chance(&f->rng, 50) ? 1 : -1;
        f->sulk_pats = 0;
        f->cue = CUE_KO;
    }
}

static bool is_pat(input_event_t ev) {
    return ev == INPUT_TOUCH || ev == INPUT_TOUCH_HOLD || ev == INPUT_TICKLE || ev == INPUT_TAP ||
           ev == INPUT_DOUBLE_TAP || ev == INPUT_BTN_A_SHORT || ev == INPUT_BTN_A_LONG;
}

// Patted while sulking: "hmph", it turns the other way, until it gives in.
// True when the event is used up.
static bool sulk_pat(face_t *f, input_event_t ev, uint32_t now) {
    if (ev != INPUT_TOUCH_HOLD && ++f->sulk_pats < SULK_PATS) {
        f->cue = CUE_HMPH;
        f->sulk_dir = -f->sulk_dir;
        face_look(f, f->sulk_dir * GLANCE_X, GLANCE_Y, now, f->sulk_until - now);
        react(f, EXPR_ANGRY, now, f->sulk_until - now);
        return true;
    }
    f->sulk_dir = 0;
    face_look(f, 0, 0, now, 0);
    f->cue = CUE_FORGIVEN;
    if (ev == INPUT_TOUCH_HOLD) return false;  // and on into the cuddle
    react(f, EXPR_LOVE, now, 3000);
    then(f, EXPR_HAPPY);
    return true;
}

static expression_t showing(const face_t *f) {
    return f->has_pending ? f->pending : f->expr;
}

void face_on_event(face_t *f, input_event_t ev, uint32_t now) {
    if (ev >= INPUT_BTN_A_PRESS && ev <= INPUT_BTN_C_PRESS) return;  // SHORT/LONG follow
    f->last_input = now;
    if (face_hung(f)) {
        hung_event(f, ev, now);
        return;
    }
    if (sulking(f, now) && is_pat(ev) && sulk_pat(f, ev, now)) return;
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
            shaken(f, now);
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
        case INPUT_TOUCH:  // a pat on the head; laughing, it just laughs on
            if (face_laughing(f)) {
                if (f->fx != FX_ROLL && (int32_t)(now + LAUGH_ON_MS - f->expr_until) > 0) {
                    f->expr_until = now + LAUGH_ON_MS;
                }
            } else if (!f->petting && showing(f) != EXPR_LOVE) {
                react(f, EXPR_HAPPY, now, 2500);
            }
            break;
        case INPUT_TOUCH_HOLD:  // petted: one sequence, timed in face_update / render
            f->petting = true;
            f->pet_since = now;
            f->pet_stage = 0;
            react(f, EXPR_BLISS, now, PETTING_MAX_MS);
            break;
        case INPUT_TOUCH_END:
            if (f->petting && f->pet_stage == 3) {  // at its height: the finale
                face_fx(f, FX_HEARTS, NULL, now, HEARTS_MS);
                f->cue = CUE_HEARTS;
            } else if (f->petting && f->pet_stage == 2) {  // melted: snaps back into shape
                react(f, EXPR_SURPRISED, now, 700);
                then(f, EXPR_HAPPY);
            } else {
                react(f, EXPR_HAPPY, now, 3000);
            }
            f->petting = false;
            break;
        case INPUT_TICKLE: {  // a laugh; tickled again straight after, it rolls about
            if (f->fx == FX_ROLL) break;  // rolling already
            const bool again = f->giggled && now - f->giggled_at < GIGGLE_AGAIN_MS;
            if (again && (!f->rolled || now - f->rolled_at >= ROLL_AGAIN_MS)) {
                f->giggled = false;
                f->rolled = true;
                f->rolled_at = now;
                face_fx(f, FX_ROLL, NULL, now, ROLL_MS);
                f->cue = CUE_ROFL;
                break;
            }
            // Laughing already: no blink, no new face, it just keeps laughing.
            if (face_laughing(f)) {
                f->expr_until = now + 3000;
            } else {
                react(f, EXPR_LAUGH, now, 3000);
            }
            f->giggled = true;
            f->giggled_at = now;
            break;
        }
        case INPUT_CLAP:  // startled, then fine
            react(f, EXPR_SURPRISED, now, TRANSITION_HOLD_MS);
            then(f, EXPR_HAPPY);
            break;
        case INPUT_DOUBLE_CLAP:  // party: main.c starts a song
            react(f, EXPR_EXCITED, now, 2500);
            break;
        case INPUT_LOUD:  // shouted at
            react(f, EXPR_SCARED, now, SCARED_HOLD_MS);
            then(f, EXPR_SAD);
            f->cue = CUE_SCARED;
            break;
        case INPUT_BLOW:  // squints into the draught, then it makes it sneeze
            react(f, EXPR_NEUTRAL, now, BLOWN_MS + SNEEZE_MS + 1500);
            f->idle = IDLE_BLOWN;
            f->idle_start = now;
            f->idle_dir = rng_chance(&f->rng, 50) ? 1 : -1;
            f->cue = CUE_WIND;
            break;
        case INPUT_HEAR:  // someone is talking: wide eyes, looking up at them
            react(f, EXPR_SURPRISED, now, LISTEN_MAX_MS);
            face_look(f, 0, -GLANCE_Y, now, LISTEN_MAX_MS);
            break;
        case INPUT_TALK:  // ...and it answers (main.c babbles)
            face_look(f, 0, 0, now, 0);
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
        case FX_HANG:
            f->petting = false;
            f->pet_stage = 0;
            f->hang_fixes = 0;
            react(f, EXPR_NEUTRAL, now, UINT32_MAX / 4);
            break;
        case FX_HEARTS:
            react(f, EXPR_LOVE, now, ms + 1200);
            then(f, EXPR_HAPPY);
            break;
        case FX_BOOT:
            react(f, EXPR_NEUTRAL, now, ms);
            then_for(f, EXPR_HAPPY, 2000);
            break;
        case FX_ROLL:
            react(f, EXPR_LAUGH, now, ms + 1500);
            break;
        case FX_REBOOT:
        case FX_NONE:
            break;
    }
}

void face_fx_stop(face_t *f) {
    if (f->fx == FX_SING || face_hung(f)) f->expr_until = f->now;  // let the mood machine back in
    f->fx = FX_NONE;
}

void face_hang(face_t *f, uint32_t now) {
    face_fx(f, FX_HANG, NULL, now, 0);
}

bool face_laughing(const face_t *f) {
    return f->fx == FX_ROLL ||
           (showing(f) == EXPR_LAUGH && f->reacting && (int32_t)(f->expr_until - f->now) > 0);
}

void face_boot(face_t *f, uint32_t now) {
    face_fx(f, FX_BOOT, NULL, now, BOOT_MS);
}

void face_talk(face_t *f, int syllables, uint32_t now) {
    if (syllables < 1) syllables = 1;
    f->talk_since = now;
    f->talk_until = now + (uint32_t)syllables * TALK_SYLLABLE_MS + 100;
}

void face_start_idle(face_t *f, idle_kind_t kind, uint32_t now) {
    f->idle = kind;
    f->idle_start = now;
    f->idle_mark = false;
    f->idle_dir = 1;
    switch (kind) {
        case IDLE_FLY: f->cue = CUE_BUZZ; break;
        case IDLE_SNOW:
            f->cue = CUE_SNOW;
            face_set_expression(f, EXPR_HAPPY, now, SNOW_MS);
            break;
        case IDLE_SNEEZE: f->cue = CUE_SNEEZE; break;
        case IDLE_HUM: f->cue = CUE_HUM; break;
        case IDLE_BLOWN: f->cue = CUE_WIND; break;
        case IDLE_STRETCH: f->cue = CUE_STRETCH; break;
        case IDLE_GLANCE:
        case IDLE_YAWN:
        case IDLE_NONE: break;
    }
}

bool face_hung(const face_t *f) {
    return f->fx == FX_HANG || f->fx == FX_REBOOT;
}

// lt: ms into the heart overload. 700 ms, speeding up to 400.
static uint32_t beat_period(uint32_t lt) {
    return 700 - (lt < 3000 ? lt : 3000) / 10;
}

uint32_t face_heartbeat_ms(const face_t *f) {
    if (!f->petting || f->pet_stage != 3) return 0;
    return beat_period(f->now - f->pet_since - PET_LOVE_MS);
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
    if (f->night || face_hung(f)) {  // asleep or frozen: feels nothing
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
    if (rng_chance(&f->rng, 4)) {
        face_start_idle(f, IDLE_FLY, now);
        return;
    }
    if (rng_chance(&f->rng, 4)) {
        face_start_idle(f, IDLE_SNOW, now);
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
            face_set_expression(f, f->after, now, f->after_ms);
        } else if (sulking(f, now)) {
            // Sulking: cross, and looking the other way.
            face_set_expression(f, EXPR_ANGRY, now, f->sulk_until - now);
            face_look(f, f->sulk_dir * GLANCE_X, GLANCE_Y, now, f->sulk_until - now);
        } else {
            f->sulk_dir = 0;
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

    // Petting goes deeper the longer it lasts (the picture follows the
    // time; the stages are for the cues and for letting go).
    if (f->petting) {
        const uint32_t t = now - f->pet_since;
        static const uint32_t AT[4] = {PET_RUB_MS, PET_MELT_MS, PET_LOVE_MS, PET_FINALE_MS};
        static const face_cue_t CUE[4] = {CUE_BLISS, CUE_MELT, CUE_OVERLOAD, CUE_HEARTS};
        if (t >= PETTING_MAX_MS) {  // a stuck pad, or a very long cuddle
            f->petting = false;
            f->pet_stage = 0;
        } else if (f->pet_stage < 4 && t >= AT[f->pet_stage]) {
            f->cue = CUE[f->pet_stage];
            if (++f->pet_stage == 4) {  // still held: the finale, then content hearts
                face_fx(f, FX_HEARTS, NULL, now, HEARTS_MS);
                react(f, EXPR_LOVE, now, PETTING_MAX_MS - t);
            }
        }
    }

    if (f->fx == FX_HANG && now - f->fx_since >= FACE_HANG_MAX_MS) reboot(f, now);  // gave up waiting
    if (f->fx == FX_REBOOT && reached(now, f->fx_until)) {
        f->fx = FX_NONE;
        react(f, EXPR_EXCITED, now, 2500);
        then(f, EXPR_HAPPY);
        f->cue = CUE_BOOTED;
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
        case IDLE_FLY: {  // the eyes follow it; it lands, gets blinked off
            const uint32_t t = now - f->idle_start;
            int x, y;
            fly_pos(t, &x, &y);
            int tx = (x - 64) * GLANCE_X / 50, ty = (y - EYE_CY) * GLANCE_Y / 15;
            tx = tx > GLANCE_X ? GLANCE_X : tx < -GLANCE_X ? -GLANCE_X : tx;
            ty = ty > GLANCE_Y ? GLANCE_Y : ty < -GLANCE_Y ? -GLANCE_Y : ty;
            face_look(f, tx, ty, now, 400);
            if (t >= FLY_SHOO_AT && !f->idle_mark) {
                f->idle_mark = true;
                start_blink(f, now);
            }
            if (t >= FLY_MS) {
                f->idle = IDLE_NONE;
                face_look(f, 0, 0, now, 0);
                schedule_idle(f, now);
            }
            break;
        }
        case IDLE_SNOW: {  // looking up, swaying with the flakes
            const uint32_t t = now - f->idle_start;
            face_look(f, swave(t, 2400, GLANCE_X / 2), -GLANCE_Y, now, 400);
            if (t >= SNOW_MS) {
                face_look(f, 0, 0, now, 0);
                if (rng_chance(&f->rng, 50)) {  // one landed on its nose
                    f->idle = IDLE_SNEEZE;
                    f->idle_start = now;
                    f->cue = CUE_SNEEZE;
                } else {
                    f->idle = IDLE_NONE;
                    schedule_idle(f, now);
                }
            }
            break;
        }
        case IDLE_BLOWN: {  // pushed aside by the wind, shaking in it; then the sneeze
            const uint32_t t = now - f->idle_start;
            face_look(f, f->idle_dir * GLANCE_X + (int)(now / 40 % 3) - 1, (int)(now / 60 % 2), now, 300);
            if (t >= BLOWN_MS) {
                face_look(f, 0, 0, now, 0);
                f->idle = IDLE_SNEEZE;
                f->idle_start = now;
                f->cue = CUE_SNEEZE;
            }
            break;
        }
        case IDLE_YAWN:
        case IDLE_SNEEZE:
        case IDLE_HUM:
        case IDLE_STRETCH: {
            const uint32_t len = f->idle == IDLE_YAWN      ? YAWN_MS
                                 : f->idle == IDLE_SNEEZE  ? SNEEZE_MS
                                 : f->idle == IDLE_STRETCH ? STRETCH_MS
                                                           : HUM_MS;
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
        case EXPR_LOVE:
        case EXPR_EXCITED: return ANIM_BOUNCE_MS;
        case EXPR_ANGRY: return ANIM_TREMBLE_MS;
        case EXPR_SAD: return ANIM_TEAR_MS;
        case EXPR_SURPRISED: return ANIM_POP_MS;
        case EXPR_DIZZY: return DIZZY_MS;  // spins for as long as it shows
        case EXPR_KO: return KO_MS;
        case EXPR_LAUGH:  // hops for as long as it laughs
        case EXPR_SCARED:
        case EXPR_FURIOUS:
        case EXPR_BLISS: return UINT32_MAX;  // moves for as long as it shows
        default: return 0;
    }
}

bool face_is_animating(const face_t *f) {
    if (f->fx != FX_NONE) return true;
    if ((int32_t)(f->talk_until - f->now) > 0) return true;
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
    // Squinting into the wind.
    if (f->idle == IDLE_BLOWN) open = open * 110 / 256;
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

static void draw_heart_c(gfx_t *g, int cx, int cy, int s, gfx_color_t c) {
    const int r = s / 4 + 1;
    gfx_fill_circle(g, cx - s / 4, cy - s / 6, r, c);
    gfx_fill_circle(g, cx + s / 4, cy - s / 6, r, c);
    gfx_fill_triangle(g, cx - s / 2, cy - s / 10, cx + s / 2, cy - s / 10, cx, cy + s / 2, c);
}

static void draw_heart(gfx_t *g, int cx, int cy, int s) {
    draw_heart_c(g, cx, cy, s, GFX_WHITE);
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

// Flips a pixel (drawing coordinates): shows on black and on an eye alike.
static void flip_pixel(gfx_t *g, int x, int y) {
    gfx_pixel(g, x, y, gfx_get_pixel(g, x + g->ox, y + g->oy) ? GFX_BLACK : GFX_WHITE);
}

// Power-on: a line streaks across, pinches into two bars, the bars spring
// open into eyes, and it looks left, right, back.
static void draw_boot(const face_t *f, gfx_t *g) {
    const uint32_t t = f->now - f->fx_since;
    if (t < 250) {
        const int w = 4 + (int)t * 58 / 250;
        gfx_fill_rect(g, 64 - w, EYE_CY - 1, 2 * w, 2, GFX_WHITE);
        return;
    }
    if (t < 450) {  // the line pinches into the two eyes: each half shrinks onto one
        const int k = (int)(t - 250) * 256 / 200;
        static const int8_t FROM[2][2] = {{2, 63}, {64, 125}};
        for (int s = 0; s < 2; s++) {
            const int cx = s ? EYE_R_CX : EYE_L_CX;
            const int l = FROM[s][0] + (cx - EYE_W / 2 - FROM[s][0]) * k / 256;
            const int r = FROM[s][1] + (cx + EYE_W / 2 - FROM[s][1]) * k / 256;
            gfx_fill_rrect(g, l, EYE_CY - 2, r - l, 4, 2, GFX_WHITE);
        }
        return;
    }
    int h = EYE_H, dx = 0;
    if (t < 1000) {  // spring open, overshooting
        const int u = (int)(t - 450);
        h = u < 180 ? 4 + (EYE_H + 6 - 4) * u / 180 : EYE_H + swave((uint32_t)(u - 180), 360, 6) * (550 - u) / 370;
    } else if (t < BOOT_MS) {  // where am I?
        const int u = (int)(t - 1000);
        dx = u < 200 ? -GLANCE_X * u / 200 : u < 400 ? -GLANCE_X + 2 * GLANCE_X * (u - 200) / 200 : u < 600 ? GLANCE_X - GLANCE_X * (u - 400) / 200 : 0;
    }
    draw_pill(g, EYE_L_CX + dx, EYE_CY, EYE_W, h, OPEN_FULL);
    draw_pill(g, EYE_R_CX + dx, EYE_CY, EYE_W, h, OPEN_FULL);
}

// Waking: stretched tall and thin, trembling; snapped squat; wobbling back.
static void draw_stretch(const face_t *f, gfx_t *g, int lx, int rx, int ey) {
    const int st = (int)(f->now - f->idle_start);
    int h, w;
    if (st < 900) {
        const int k = st * 256 / 900;
        h = 8 + (EYE_H + 14 - 8) * k / 256;
        w = EYE_W - 10 * k / 256;
        if (st > 650) w += (st / 40) % 2 ? 1 : -1;  // straining
    } else if (st < 1150) {
        const int k = (st - 900) * 256 / 250;
        h = EYE_H + 14 - 32 * k / 256;
        w = EYE_W - 10 + 18 * k / 256;
    } else {
        const int decay = STRETCH_MS - st;
        h = EYE_H - 18 * decay / (STRETCH_MS - 1150) + swave((uint32_t)st, 280, 8) * decay / (STRETCH_MS - 1150);
        w = EYE_W + 8 * decay / (STRETCH_MS - 1150);
    }
    draw_pill(g, lx, ey, w, h, OPEN_FULL);
    draw_pill(g, rx, ey, w, h, OPEN_FULL);
}

// A fly: a body with two wings, flapping (up) or folded.
static void draw_fly(gfx_t *g, int x, int y, bool flap) {
    const int wy = flap ? y - 4 : y - 2, wx = flap ? 4 : 3;
    gfx_fill_circle(g, x, y, 4, GFX_BLACK);  // shows up over an eye
    gfx_fill_ellipse(g, x - wx, wy, 4, 3, GFX_BLACK);
    gfx_fill_ellipse(g, x + wx, wy, 4, 3, GFX_BLACK);
    gfx_fill_circle(g, x, y, 2, GFX_WHITE);
    gfx_fill_ellipse(g, x - wx, wy, 2, 1, GFX_WHITE);
    gfx_fill_ellipse(g, x + wx, wy, 2, 1, GFX_WHITE);
    gfx_pixel(g, x + 3, y + 1, GFX_WHITE);  // its head
}

static void draw_snow(gfx_t *g, uint32_t t) {
    for (int k = 0; k < SNOW_FLAKES; k++) {
        const uint32_t h = (uint32_t)(k + 1) * 2654435761u;
        const int speed = 9 + (int)((h >> 7) % 12);  // px per second
        const int start = (int)((h >> 13) % 1800);
        if ((int)t < start) continue;
        const int y = -3 + ((int)t - start) * speed / 1000;
        const int x = 6 + (int)(h % 116) + swave(t + h % 997, 1800 + h % 700, 4);
        const int arm = (h >> 20) & 1 ? 2 : 1;
        flip_pixel(g, x, y);
        for (int a = 1; a <= arm; a++) {
            flip_pixel(g, x - a, y);
            flip_pixel(g, x + a, y);
            flip_pixel(g, x, y - a);
            flip_pixel(g, x, y + a);
        }
        if (arm == 2) {
            flip_pixel(g, x - 1, y - 1);
            flip_pixel(g, x + 1, y + 1);
            flip_pixel(g, x - 1, y + 1);
            flip_pixel(g, x + 1, y - 1);
        }
    }
}

// Gusts streaking across in the wind's direction.
static void draw_wind(gfx_t *g, uint32_t t, int dir) {
    for (int k = 0; k < 7; k++) {
        const int y = 6 + k * 8 + (k % 2) * 3, len = 10 + (k * 7) % 9;
        int x = (int)((t * (uint32_t)(220 + k * 30) / 1000 + (uint32_t)k * 41) % 170) - 20;
        if (dir < 0) x = GFX_W - 1 - x - len;
        for (int i = 0; i < len; i++) {
            if ((i / 3) % 3 != 2) flip_pixel(g, x + i, y);
        }
    }
}

// A happy arch of any size, bottom edge at `bottom` (draw_smile_eye is the
// full-size one).
static void draw_arch(gfx_t *g, int cx, int bottom, int w, int h) {
    const int cy = bottom - h / 2;
    gfx_fill_rrect(g, cx - w / 2, cy - h / 2, w, h, EYE_RAD, GFX_WHITE);
    gfx_fill_ellipse(g, cx, cy + h * 16 / 30, w * 17 / 26, h * 13 / 30, GFX_BLACK);
}

// The lowest lit pixel of draw_arch's arch at dx from its centre, +1.
static int arch_edge(int cx, int bottom, int w, int h, int dx) {
    (void)cx;
    const int cy = bottom - h / 2, ecy = cy + h * 16 / 30, ery = h * 13 / 30, erx = w * 17 / 26;
    if (dx < 0) dx = -dx;
    if (dx >= erx) return bottom;
    return ecy - ery * gfx_isqrt(erx * erx - dx * dx) / erx;
}

// "(" (dir -1) or ")" (dir 1) of radius r: a purr rippling off the side.
static void draw_ripple(gfx_t *g, int x, int cy, int r, int dir) {
    for (int dy = -r; dy <= r; dy++) {
        const int bow = r - gfx_isqrt(r * r - dy * dy);
        gfx_pixel(g, x - dir * bow, cy + dy, GFX_WHITE);
    }
}

static void draw_blush(gfx_t *g, int cx, int y) {
    for (int k = 0; k < 3; k++) gfx_line(g, cx - 9 + k * 6, y + 4, cx - 6 + k * 6, y, GFX_WHITE);
}

// Petting, by how long it has gone on (pt, ms since TOUCH_HOLD):
//   squished: the arches flatten under the hand and spring a little;
//   nuzzling: swaying into the hand, purring (shiver, ripples), blushing;
//   melting:  the arches sag, drip and puddle;
//   overload: the eyes are hearts beating ever faster, sparkles around.
static void draw_petting(const face_t *f, gfx_t *g, int lx, int rx, int ey, uint32_t pt) {
    const int bottom = ey + EYE_H / 2 - 8;
    const int purr = (int)(f->now / 45 % 2);
    if (pt < PET_RUB_MS) {
        const int s = pt < 250 ? (int)pt * 256 / 250 : 256;
        const int spring = pt < 1200 ? wave(pt, 300, 4) * (1200 - (int)pt) / 1200 : 0;
        const int h = EYE_H - 14 * s / 256 + spring, w = EYE_W + 2 + 8 * s / 256 - spring / 2;
        draw_arch(g, lx, bottom, w, h);
        draw_arch(g, rx, bottom, w, h);
    } else if (pt < PET_MELT_MS) {
        const uint32_t rt = pt - PET_RUB_MS;
        const int sway = wave(rt, 1100, 7);
        draw_arch(g, lx + sway, bottom + purr, EYE_W + 10, EYE_H - 14);
        draw_arch(g, rx + sway, bottom + purr, EYE_W + 10, EYE_H - 14);
        draw_blush(g, lx + sway, bottom + 4);
        draw_blush(g, rx + sway, bottom + 4);
        for (int k = 0; k < 2; k++) {  // ripples off the sides, moving outward
            const int q = (int)((rt / 120 + (uint32_t)k * 3) % 6);
            if (q < 4) {
                draw_ripple(g, 10 - q, ey, 4 + q, -1);
                draw_ripple(g, 117 + q, ey, 4 + q, 1);
            }
        }
    } else if (pt < PET_LOVE_MS) {
        const uint32_t mt = pt - PET_MELT_MS;
        const int m = (int)mt * 256 / (PET_LOVE_MS - PET_MELT_MS);
        const int h = EYE_H - 14 - 8 * m / 256, w = EYE_W + 10 + 6 * m / 256;
        const int puddle_y = GFX_H - 6;
        for (int s = 0; s < 2; s++) {
            const int cx = s ? rx : lx;
            draw_arch(g, cx, bottom + 3 * m / 256, w, h);
            // Drops swelling on the arch's lower edge, falling off into the puddle.
            static const int8_t DX[3] = {-11, 2, 12};
            static const int16_t PERIOD[3] = {700, 950, 820};
            for (int k = 0; k < 3; k++) {
                const int x = cx + DX[k] + s;
                const int y0 = arch_edge(cx, bottom + 3 * m / 256, w, h, x - cx);
                const int q = (int)((mt + (uint32_t)(k * 300 + s * 170)) % (uint32_t)PERIOD[k]);
                const int grow = PERIOD[k] * 6 / 10;
                if (q < grow) {  // hanging, swelling
                    const int len = 1 + 5 * q / grow;
                    gfx_fill_rect(g, x, y0, 2, len, GFX_WHITE);
                    gfx_fill_circle(g, x, y0 + len, 1 + q * 2 / grow, GFX_WHITE);
                } else {  // let go: falling
                    const int y = y0 + 6 + (q - grow) * (puddle_y - y0) / (PERIOD[k] - grow);
                    if (y < puddle_y - 2) gfx_fill_circle(g, x, y, 1, GFX_WHITE);
                }
            }
            gfx_fill_ellipse(g, cx, puddle_y, 3 + 13 * m / 256, 1, GFX_WHITE);
        }
    } else {
        const uint32_t lt = pt - PET_LOVE_MS;
        const uint32_t p = lt % beat_period(lt);
        const int beat = p < 90 ? 6 : p < 160 ? 1 : p < 240 ? 4 : 0;  // ba-dum
        draw_heart(g, lx + purr, ey, 30 + beat);
        draw_heart(g, rx + purr, ey, 30 + beat);
        for (int k = 0; k < 6; k++) {  // sparkles popping up around
            uint32_t h = ((lt / 260) * 6 + (uint32_t)k + 1) * 2654435761u;
            h ^= h >> 15;
            const int x = 8 + (int)(h % 112), y = 6 + (int)((h >> 8) % 50);
            if ((x > 14 && x < 58) || (x > 70 && x < 114)) continue;  // not on the hearts
            draw_star(g, x, y, 1 + (int)(h >> 20) % 2);
        }
    }
}

// The finale: hearts flying out from the middle, with a pop.
static void draw_heart_burst(gfx_t *g, uint32_t ft) {
    static const int16_t DIR[8][2] = {{256, 0}, {181, 181}, {0, 256}, {-181, 181},
                                      {-256, 0}, {-181, -181}, {0, -256}, {181, -181}};
    if (ft < 120) {
        invert(g);
        return;
    }
    for (int ring = 0; ring < 2; ring++) {
        const int d = 4 + (int)ft * (ring ? 40 : 70) / HEARTS_MS;
        const int s = 12 - (int)ft * 6 / HEARTS_MS - ring * 3;
        for (int k = 0; k < 8; k++) {
            const int a = (k + ring * 4) % 8;  // the inner ring sits between
            const int x = 64 + DIR[a][0] * d * 2 / 256 + (ring ? DIR[(a + 1) % 8][0] * 6 / 256 : 0);
            const int y = 31 + DIR[a][1] * d / 256;
            draw_heart_c(g, x, y, s + 3, GFX_BLACK);
            draw_heart_c(g, x, y, s, GFX_WHITE);
        }
    }
}

// Gritted teeth: a bar with gaps.
static void draw_teeth(gfx_t *g, int cx, int y) {
    gfx_fill_rrect(g, cx - 12, y, 25, 8, 2, GFX_WHITE);
    gfx_hline(g, cx - 11, cx + 11, y + 4, GFX_BLACK);
    for (int x = cx - 8; x <= cx + 8; x += 4) gfx_fill_rect(g, x, y + 1, 1, 6, GFX_BLACK);
}

// Puffs of steam rising off the head, `n` per side.
static void draw_steam(gfx_t *g, int lx, int rx, int dy, uint32_t t, int n) {
    for (int k = 0; k < n; k++) {
        const int q = (int)((t / 35 + (uint32_t)k * 24 / (uint32_t)n) % 24);
        const int r = 1 + q / 6, y = 17 + dy - q / 3;
        for (int s = 0; s < 2; s++) {
            const int x = s ? rx + 8 + q / 6 : lx - 8 - q / 6;
            gfx_fill_circle(g, x, y, r, GFX_WHITE);
            if (r > 1) gfx_fill_circle(g, x, y, r - 1, GFX_BLACK);
        }
    }
}

// Slides rows y0..y0+h-1 sideways by dx, inside the burn-in margin (no wrap).
static void shift_band(gfx_t *g, int y0, int h, int dx) {
    uint8_t row[GFX_W];
    const int m = BURNIN_MAX_SHIFT_PX;
    for (int y = y0; y < y0 + h && y < GFX_H; y++) {
        for (int x = 0; x < GFX_W; x++) row[x] = (uint8_t)gfx_get_pixel(g, x, y);
        for (int x = m; x < GFX_W - m; x++) {
            const int sx = x - dx;
            const bool on = sx >= m && sx < GFX_W - m && row[sx];
            gfx_pixel(g, x - g->ox, y - g->oy, on ? GFX_WHITE : GFX_BLACK);
        }
    }
}

// Frozen: the face as it stopped (one eye mid-blink, torn), and a dialog
// with a stuck progress bar. Each shake jolts it and moves the bar on.
static void draw_hang(const face_t *f, gfx_t *g) {
    const uint32_t t = f->now - f->fx_since;
    const bool jolted = f->hang_fixes > 0 && f->now - f->jolt_at < JOLT_MS;
    const int jx = jolted ? (int)(f->now / 30 % 5) - 2 : 0, jy = jolted ? (int)(f->now / 50 % 5) - 2 : 0;
    draw_pill(g, EYE_L_CX + 2 + jx, EYE_CY - 6 + jy, EYE_W, EYE_H - 14, OPEN_FULL);
    draw_pill(g, EYE_R_CX + 2 + jx, EYE_CY - 6 + jy, EYE_W, EYE_H - 14, OPEN_FULL / 3);
    shift_band(g, 14 + (int)(f->fx_since % 5), 4, 7);
    if (jolted) shift_band(g, 22 + (int)(f->now / 40 % 10), 3, (f->now / 20) % 2 ? 9 : -9);

    const int bx = 10 + jx, by = 42 + jy, bw = 108;
    gfx_fill_rect(g, bx, by, bw, 18, GFX_BLACK);
    gfx_rect(g, bx, by, bw, 18, GFX_WHITE);
    const bool hint = t >= HANG_HINT_MS && (t / 600) % 2;
    font_draw_centered(g, 64 + jx, by + 3, hint ? "SHAKE ME!" : "NOT RESPONDING", 1, GFX_WHITE);
    const int pct = 23 + f->hang_fixes * 25;
    gfx_rect(g, bx + 6, by + 12, bw - 12, 4, GFX_WHITE);
    gfx_fill_rect(g, bx + 6, by + 12, (bw - 12) * pct / 100, 4, GFX_WHITE);
    if (jolted && (f->now / 60) % 2) invert(g);
}

// Shaken free: dark, a boot screen filling up, then "OK!".
static void draw_reboot(const face_t *f, gfx_t *g) {
    static const char *const LOADING[4] = {"LOADING", "LOADING.", "LOADING..", "LOADING..."};
    const uint32_t t = f->now - f->fx_since;
    if (t < 400) return;  // powered down
    if (t < 2100) {
        font_draw_centered(g, 64, 12, "YETI-7 BIOS", 1, GFX_WHITE);
        int fill = (int)(t - 400) * 92 / 1500;
        if (fill > 92) fill = 92;
        gfx_rect(g, 16, 28, 96, 7, GFX_WHITE);
        gfx_fill_rect(g, 18, 30, fill, 3, GFX_WHITE);
        font_draw(g, 40, 44, LOADING[(t / 250) % 4], 1, GFX_WHITE);
        return;
    }
    font_draw_centered(g, 64, 14, "OK!", 3, GFX_WHITE);
    font_draw_centered(g, 64, 46, "ALL SYSTEMS GO", 1, GFX_WHITE);
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
    if (f->fx == FX_HANG) {
        draw_hang(f, g);
        return;
    }
    if (f->fx == FX_REBOOT) {
        draw_reboot(f, g);
        return;
    }
    if (f->fx == FX_BOOT) {
        draw_boot(f, g);
        return;
    }
    if (f->fx == FX_ROLL) {
        // Laughing itself silly: rolls off to the left, back in from the right.
        face_t c = *f;
        c.fx = FX_NONE;
        const uint32_t rt = f->now - f->fx_since;
        const int p = (int)(rt * 256 / ROLL_MS);  // eased: starts slow, rushes, settles
        const int x = -GFX_W * (3 * p * p - 2 * p * p * p / 256) / 65536;
        const int y = -abs(swave(rt, 320, 5));
        const int ox = g->ox, oy = g->oy;
        for (int k = 0; k < 2; k++) {
            gfx_set_offset(g, ox + x + k * GFX_W, oy + y);
            face_render(&c, g);
        }
        gfx_set_offset(g, ox, oy);
        return;
    }
    bool flash = false;  // invert the whole picture this frame

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

    if (f->idle == IDLE_STRETCH) {
        draw_stretch(f, g, lx, rx, ey);
        return;
    }

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
            // Tears of joy flying off the outer corners.
            for (int s = 0; s < 2 && moving; s++) {
                const int u = (int)((t + (uint32_t)s * 300) % 700);
                const int x = (s ? rx + EYE_W / 2 + 2 : lx - EYE_W / 2 - 2) + (s ? 1 : -1) * u * 12 / 700;
                const int y = ey - 8 + (-14 * u + 28 * u * u / 700) / 700;
                if (x >= BURNIN_MAX_SHIFT_PX + 3 && x < GFX_W - BURNIN_MAX_SHIFT_PX - 3) draw_tear(g, x, y);
            }
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
        case EXPR_FURIOUS: {
            // Slit eyes shaking hard, gritted teeth, steam; boiling over at first.
            const int j = (t / 40) % 2 ? 2 : -2;
            draw_cut_eye(g, lx + j, ey + 3, EYE_W, EYE_H - 12, open, -1, true);
            draw_cut_eye(g, rx + j, ey + 3, EYE_W, EYE_H - 12, open, 1, true);
            draw_teeth(g, mx + j, my + 2);
            draw_steam(g, lx, rx, dy, t, f->rage >= RAGE_KO - 1 ? 3 : 2);
            flash = t < FURY_FLASH_MS && (t / 80) % 2;
            break;
        }
        case EXPR_BLISS:
            // Not petted (B held through the faces): show it nuzzling.
            draw_petting(f, g, lx, rx, ey, f->petting ? f->now - f->pet_since : PET_RUB_MS + t);
            break;
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

    // A yawn: the only time it has a mouth. Or answering someone.
    if (yawn) draw_yawn(g, mx, my, yawn);
    if ((int32_t)(f->talk_until - f->now) > 0) {
        if ((f->now - f->talk_since) % TALK_SYLLABLE_MS < TALK_SYLLABLE_MS * 6 / 10) {
            gfx_fill_ellipse(g, mx, my + 3, 5, 4, GFX_WHITE);
            gfx_fill_ellipse(g, mx, my + 3, 3, 2, GFX_BLACK);
        } else {
            gfx_fill_rect(g, mx - 4, my + 3, 9, 2, GFX_WHITE);
        }
    }

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

    if (f->idle == IDLE_FLY) {
        const uint32_t ft = f->now - f->idle_start;
        int x, y;
        fly_pos(ft, &x, &y);
        draw_fly(g, x + dx, y, ft < FLY_SIT_AT || ft >= FLY_OFF_AT ? (f->now / 30) % 2 : (ft / 500) % 4 == 0);
    } else if (f->idle == IDLE_SNOW) {
        draw_snow(g, f->now - f->idle_start);
    } else if (f->idle == IDLE_BLOWN) {
        draw_wind(g, f->now - f->idle_start, f->idle_dir);
    }

    if (flash) invert(g);

    const uint32_t ft = f->now - f->fx_since;
    if (f->fx == FX_ALARM) {
        // Siren: the whole screen flashes for a second, then the sign blinks.
        if ((ft / 250) % 2 == 0) draw_warning(g);
        if (ft < 1200 && (ft / 150) % 2) invert(g);
    } else if (f->fx == FX_HEARTS) {
        draw_heart_burst(g, ft);
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
