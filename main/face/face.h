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
#include "pet/pet.h"

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
    EXPR_LOWBAT,  // never picked by the mood machine; battery code sets it
    EXPR_COUNT
} expression_t;

// The ones the mood machine picks from.
#define EXPR_MOOD_COUNT EXPR_LAUGH

typedef enum {
    IDLE_NONE = 0,
    IDLE_GLANCE,  // look somewhere, hold, come back
    IDLE_YAWN,
    IDLE_SNEEZE,  // eyes squeeze, a jolt, "achoo"
    IDLE_HUM,     // music notes drifting up
} idle_kind_t;

// A full-face effect on top of (or instead of) the expression.
typedef enum {
    FX_NONE = 0,
    FX_ALARM,   // the screen flashes, a warning sign blinks
    FX_GLITCH,  // the picture tears, a line of the log flickers up
    FX_SING,    // eyes shut happily, mouth going, notes flying
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

    uint8_t needs;  // PET_NEED_* bits: drives the mood and the thought bubble
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

void face_set_needs(face_t *f, uint8_t needs);  // PET_NEED_* bits

// Starts an effect for ms (0 = until stopped). `text` must outlive it.
void face_fx(face_t *f, face_fx_t fx, const char *text, uint32_t now, uint32_t ms);
void face_fx_stop(face_t *f);
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
    uint8_t needs;            // PET_NEED_* bits
} mood_ctx_t;

// Next spontaneous expression and how long it holds.
expression_t mood_pick(rng_t *rng, expression_t current, const mood_ctx_t *ctx,
                       uint32_t *hold_ms);
