// Shake and tilt from raw LIS3DH samples, and tap / double tap from the
// LIS3DH's single-click flags, with no hardware in it. Pure C (host-tested).

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "input/input_event.h"

typedef enum { TILT_LEVEL = 0, TILT_LEFT, TILT_RIGHT } tilt_t;

typedef struct {
    uint32_t hits[ACCEL_SHAKE_HITS];  // ring of recent shake-sample times
    int nhits;                        // how many slots are filled
    int next;
    uint32_t quiet_until;  // after a shake: no tap, tilt or second shake
    bool quiet;
    uint32_t active_until;  // keep sampling fast until then
    bool active;

    // Taps: the first accepted click opens a window; a second accepted click
    // inside it is a DOUBLE_TAP, otherwise the window closing is a TAP.
    bool tap_pending;
    uint32_t tap_first;
    uint32_t tap_last;  // last click of any kind: ringing is a chain of close clicks
    bool tap_seen;      // tap_last is valid

    tilt_t tilt;       // reported state
    tilt_t candidate;  // state being held, waiting for ACCEL_TILT_HOLD_MS
    uint32_t candidate_since;
} motion_t;

void motion_init(motion_t *m);

// One sample in mg. Returns INPUT_SHAKE / INPUT_TILT_LEFT / INPUT_TILT_RIGHT
// or INPUT_NONE.
input_event_t motion_update(motion_t *m, int x_mg, int y_mg, int z_mg, uint32_t now);

// The LIS3DH reported a single click. Returns INPUT_DOUBLE_TAP when this
// completes a double, else INPUT_NONE (a TAP comes later from motion_tap_poll).
input_event_t motion_click(motion_t *m, uint32_t now);

// Every frame: INPUT_TAP once a lone click's double-tap window has passed.
input_event_t motion_tap_poll(motion_t *m, uint32_t now);

// True while moving, a tilt is being confirmed or a tap window is open:
// sample at ~30 Hz.
bool motion_busy(const motion_t *m, uint32_t now);

// False during and right after a shake, when the click unit fires constantly.
bool motion_tap_allowed(const motion_t *m, uint32_t now);
