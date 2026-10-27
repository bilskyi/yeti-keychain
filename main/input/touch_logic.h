// The touch pad's gestures, with no hardware in it: a touch, a hold (petting)
// and its end, a tickle (quick touches). Pure C (host-tested). touch.c
// samples the pin and calls this.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "input/button_logic.h"
#include "input/input_event.h"

typedef struct {
    button_t b;  // debounce and short/long, as for a button
    bool held;   // a TOUCH_HOLD went out: TOUCH_END follows on release
    int taps;    // quick touches so far...
    uint32_t first_tap;  // ...counted from this one
} touch_t;

void touch_init(touch_t *t);

// Feed one sample. Returns INPUT_TOUCH, INPUT_TOUCH_HOLD, INPUT_TOUCH_END,
// INPUT_TICKLE or INPUT_NONE. The touch that completes a tickle is a TICKLE
// instead of a TOUCH.
input_event_t touch_update(touch_t *t, bool touched, uint32_t now);

// True while touched or settling: sample fast.
bool touch_busy(const touch_t *t);
