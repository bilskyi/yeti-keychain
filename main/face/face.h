// The face: expressions, blinking, idle behaviours, and reactions to input.
//
// Pure C, no ESP-IDF. Time comes in as a millisecond counter and randomness
// from a seeded generator, so the host tests can run a whole day of face in a
// few milliseconds and get the same result every time.
//
//   face_init()      once
//   face_on_event()  for each input event
//   face_feel()      each accelerometer sample (gaze, upside down, falls)
//   face_update()    once per frame, before rendering
//   face_render()    draws into a gfx_t
//
// Time is a wrapping uint32_t: every comparison goes through a signed
// difference, so the 49-day wrap is harmless.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "display/gfx.h"
#include "face/rng.h"
#include "input/input_event.h"

typedef enum {
    // Moods: the mood machine drifts between these on its own.
    EXPR_NEUTRAL = 0,
    EXPR_HAPPY,
    EXPR_SLEEPY,
    EXPR_SURPRISED,
    EXPR_ANGRY,
    EXPR_LOVE,
    EXPR_SAD,
    // Reactions only: input triggers them, the mood machine never picks them.
    EXPR_LAUGH,    // happy arches, giggling hops
    EXPR_DIZZY,    // spinning spirals
    EXPR_KO,       // X X, a star circling
    EXPR_EXCITED,  // star eyes
    EXPR_WINK,
    EXPR_SCARED,   // upside down: wide trembling eyes, a sweat drop
    EXPR_FURIOUS,  // shaken again and again: slit eyes, gritted teeth, steam
    EXPR_BLISS,    // being petted: squished, nuzzling, melting, heart overload (by time)
    EXPR_LOWBAT,  // never picked by the mood machine; battery code sets it
    EXPR_COUNT
} expression_t;

// The ones the mood machine picks from.
#define EXPR_MOOD_COUNT EXPR_LAUGH

#define FACE_HANG_FIXES 3
#define FACE_HANG_MAX_MS 90000
#define FACE_REBOOT_MS 2800

typedef enum {
    IDLE_NONE = 0,
    IDLE_GLANCE,  // look somewhere, hold, come back
    IDLE_YAWN,
    IDLE_SNEEZE,  // eyes squeeze, a jolt, "achoo"
    IDLE_HUM,     // music notes drifting up
    IDLE_FLY,     // a fly buzzes about, the eyes follow it; it lands, gets blinked off
    IDLE_SNOW,    // snow falls, it looks up happily (and maybe sneezes)
    IDLE_STRETCH, // waking up: a big stretch, tall then squashed, settling
} idle_kind_t;

// A full-face effect on top of (or instead of) the expression.
typedef enum {
    FX_NONE = 0,
    FX_GLITCH,  // the picture tears, a line of the log flickers up
    FX_SING,    // eyes shut happily, mouth going, notes flying
    FX_HANG,    // frozen: "NOT RESPONDING" until shaken back to life (face_hang)
    FX_REBOOT,  // after a hang: a boot screen, then a happy face
    FX_HEARTS,  // the end of a long cuddle: a burst of hearts
    FX_BOOT,    // power-on: a line that opens into eyes, a look around (face_boot)
    FX_ROLL,    // tickled twice running: laughing so hard it rolls off the screen and back
} face_fx_t;

// Something happened that wants a sound; main.c plays it (face_take_cue).
typedef enum {
    CUE_NONE = 0,
    CUE_HI,      // picked up after a long rest
    CUE_SCARED,  // turned upside down
    CUE_RELIEF,  // the right way up again
    CUE_WHEE,    // falling, or thrown
    CUE_SNEEZE,
    CUE_HUM,
    CUE_GRUMBLE,   // shaken twice in a row: getting cross
    CUE_FURY,      // shaken three or four times: furious
    CUE_KO,        // shaken five times: knocked out, then it sulks
    CUE_HMPH,      // patted while sulking: not yet
    CUE_FORGIVEN,  // sulking is over
    CUE_BLISS,     // petting: squished, now nuzzling into the hand
    CUE_MELT,      // petting: melting
    CUE_OVERLOAD,  // petting: heart overload (main.c beats a heart meanwhile)
    CUE_HEARTS,    // petting's finale: the burst of hearts
    CUE_BUZZ,      // a fly turns up
    CUE_SNOW,      // it starts to snow
    CUE_ROFL,      // rolling with laughter
    CUE_STRETCH,   // waking up with a stretch
    CUE_JOLT,      // shaken while hung: a jolt, the bar moves
    CUE_ERROR,     // anything else while hung: an error beep
    CUE_REBOOT,    // shaken free: it powers down and reboots
    CUE_BOOTED,    // the reboot finished
} face_cue_t;

typedef struct {
    rng_t rng;

    // What is drawn, and what it changes to at the bottom of the current blink.
    expression_t expr;
    uint32_t expr_since;  // when expr was swapped in: drives its animation
    expression_t pending;
    bool has_pending;

    // Until when the current expression holds before the mood machine picks
    // again. Reactions to input set this too.
    uint32_t expr_until;
    bool reacting;  // the current expression came from an input event

    // A reaction can chain into a second expression (TAP: surprised, then happy).
    expression_t after;
    bool has_after;

    uint32_t last_input;  // feeds the mood machine's boredom

    // Blink. blink_start is only meaningful while blinking.
    bool blinking;
    uint32_t blink_start;
    uint32_t next_blink;
    bool double_blink;  // a second blink follows immediately

    // Where the eyes look, in pixels; current eases toward target.
    int look_x, look_y;
    int look_tx, look_ty;
    uint32_t look_until;  // when the target returns to centre (0 = never)

    // Idle behaviour in progress.
    idle_kind_t idle;
    uint32_t idle_start;
    uint32_t next_idle;

    bool night;
    uint32_t now;  // last face_update() time, for rendering

    face_cue_t cue;

    // Body sense (face_feel).
    bool upside;           // currently upside down
    uint32_t flip_since;   // when the orientation started to change
    bool flip_pending;
    int prev_mg[3];
    bool has_prev;
    uint32_t still_since;  // last time it moved
    int low_g;             // consecutive near-weightless samples
    uint32_t whee_until;   // no second WHEE before this

    face_fx_t fx;
    uint32_t fx_since, fx_until;  // fx_until 0 = until face_fx_stop
    const char *fx_text;          // FX_GLITCH: the log line
    uint32_t after_ms;            // how long the `after` expression holds

    // Shaking: each shake within RAGE_COOL_MS of the last makes it angrier.
    int rage;
    uint32_t rage_at;
    uint32_t sulk_until;  // after a KO it sulks, looking away, until then
    int sulk_dir;         // which way it looks while sulking
    int sulk_pats;        // pats so far towards forgiving

    // Petting (touch pad held): stage 0 squished, 1 nuzzling, 2 melting,
    // 3 heart overload, 4 content (after the finale, still held).
    bool petting;
    uint32_t pet_since;
    int pet_stage;

    bool idle_mark;  // IDLE_FLY: the fly has been blinked at

    bool giggled;  // laughed at a tickle: another one soon and it rolls
    uint32_t giggled_at;
    bool rolled;  // ...but not twice running (ROLL_AGAIN_MS)
    uint32_t rolled_at;

    uint32_t talk_since, talk_until;  // face_talk: the mouth moves until then

    // FX_HANG: shakes so far, and when the last one landed.
    int hang_fixes;
    uint32_t jolt_at;
} face_t;

void face_init(face_t *f, uint32_t seed, uint32_t now);

// Switches expression through a blink. hold_ms = how long it stays before the
// mood machine takes over again.
void face_set_expression(face_t *f, expression_t e, uint32_t now, uint32_t hold_ms);

// Night: forced sleepy, no glances, slow blinks. Leaving night wakes the face.
void face_set_night(face_t *f, bool night, uint32_t now);

void face_look(face_t *f, int dx, int dy, uint32_t now, uint32_t hold_ms);

// Show e as a reaction (the mood machine stays out of it for hold_ms).
void face_react(face_t *f, expression_t e, uint32_t now, uint32_t hold_ms);

void face_on_event(face_t *f, input_event_t ev, uint32_t now);

// One accelerometer sample in the screen frame (input/orient.h: x right,
// y up, z out). The eyes follow the tilt; upside down it panics; picked up
// after a long rest it says hi; a fall or a throw is a WHEE.
// `oriented`: the sensor is calibrated, so "upside down" can be trusted.
void face_feel(face_t *f, const int mg[3], bool oriented, uint32_t now);


// Starts an effect for ms (0 = until stopped). `text` must outlive it.
void face_fx(face_t *f, face_fx_t fx, const char *text, uint32_t now, uint32_t ms);
void face_fx_stop(face_t *f);
// Freezes: only shaking (FACE_HANG_FIXES times) brings it back, through a
// reboot; it reboots by itself after FACE_HANG_MAX_MS. While hung, every
// event goes to the face alone (main.c checks face_hung).
void face_hang(face_t *f, uint32_t now);
void face_boot(face_t *f, uint32_t now);  // the power-on animation
// Laughing at a tickle (or rolling with it): more taps keep it going.
bool face_laughing(const face_t *f);
// It answers someone who talked: the mouth moves for that many syllables.
void face_talk(face_t *f, int syllables, uint32_t now);
// Starts an idle behaviour now, whatever is going on (tests, previews).
void face_start_idle(face_t *f, idle_kind_t kind, uint32_t now);
bool face_hung(const face_t *f);  // hung or rebooting
// Petted into the heart overload: the current heartbeat period (it speeds
// up), for the sound and LED to beat along; 0 otherwise.
uint32_t face_heartbeat_ms(const face_t *f);
face_cue_t face_take_cue(face_t *f);

void face_update(face_t *f, uint32_t now);

// True while anything is moving, so the caller can drop to a slow frame rate
// the rest of the time.
bool face_is_animating(const face_t *f);

void face_render(const face_t *f, gfx_t *g);

const char *face_expression_name(expression_t e);

// --- mood machine (mood.c) ---

typedef struct {
    bool night;
    uint32_t ms_since_input;  // UINT32_MAX when there has never been any
} mood_ctx_t;

// Next spontaneous expression and how long it holds.
expression_t mood_pick(rng_t *rng, expression_t current, const mood_ctx_t *ctx,
                       uint32_t *hold_ms);
