// The face: expressions, blinking, idle behaviours, and reactions to input.
//
// Pure C, no ESP-IDF. Time comes in as a millisecond counter and randomness
// from a seeded generator, so the host tests can run a whole day of face in a
// few milliseconds and get the same result every time.
//
//   face_init()      once
//   face_on_event()  for each input event
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
    EXPR_NEUTRAL = 0,
    EXPR_HAPPY,
    EXPR_SLEEPY,
    EXPR_SURPRISED,
    EXPR_ANGRY,
    EXPR_LOVE,
    EXPR_LOWBAT,  // never picked by the mood machine; battery code sets it
    EXPR_COUNT
} expression_t;

// The ones the mood machine and BTN_A cycle through.
#define EXPR_MOOD_COUNT EXPR_LOWBAT

typedef enum {
    IDLE_NONE = 0,
    IDLE_GLANCE,  // look somewhere, hold, come back
    IDLE_YAWN,
} idle_kind_t;

typedef struct {
    rng_t rng;

    // What is drawn, and what it changes to at the bottom of the current blink.
    expression_t expr;
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
} face_t;

void face_init(face_t *f, uint32_t seed, uint32_t now);

// Switches expression through a blink. hold_ms = how long it stays before the
// mood machine takes over again.
void face_set_expression(face_t *f, expression_t e, uint32_t now, uint32_t hold_ms);

// Night: forced sleepy, no glances, slow blinks. Leaving night wakes the face.
void face_set_night(face_t *f, bool night, uint32_t now);

void face_look(face_t *f, int dx, int dy, uint32_t now, uint32_t hold_ms);

void face_on_event(face_t *f, input_event_t ev, uint32_t now);

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
