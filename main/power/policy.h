// Power policy decisions that are pure functions of time and input: when it
// sleeps, when the screen goes off, the burn-in shift, and the contrast that
// goes with them.
//
// It sleeps ("night") after SLEEP_AFTER_MS without input, or while lying face
// down; the screen is off only face down. Any input wakes it, and so does
// picking it up.
//
// Pure C, no ESP-IDF (host-tested). power.c does the hardware side.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "input/input_event.h"

typedef struct {
    bool night;
    bool face_down;           // lying screen down: the screen is off
    uint64_t last_activity;
    uint64_t down_since;      // screen-down reading began, 0 = not now
    int prev_mg[3];
    bool has_prev;
} policy_t;

void policy_init(policy_t *p, uint64_t now_ms);

// Something happened (input, a menu or game in use): awake for another
// SLEEP_AFTER_MS.
void policy_note_activity(policy_t *p, uint64_t now_ms);
void policy_on_event(policy_t *p, input_event_t ev, uint64_t now_ms);

// An accelerometer sample in the screen frame (input/orient.h: z = out of
// the screen). Face-down only counts when `oriented` (calibrated); a pick-up
// counts either way.
void policy_feel(policy_t *p, const int mg[3], bool oriented, uint64_t now_ms);

// Recomputes p->night. Returns true when it changed.
bool policy_update(policy_t *p, uint64_t now_ms);

// The screen should be off (face down).
bool policy_screen_off(const policy_t *p);

// The contrast for now: `day` while awake, CONTRAST_NIGHT asleep.
uint8_t policy_contrast(const policy_t *p, uint8_t day);

// Burn-in shift: the offset the whole face is drawn at, stepping through a
// small loop every BURNIN_SHIFT_MS. Never beyond +-BURNIN_MAX_SHIFT_PX.
void policy_burnin_offset(uint64_t now_ms, int *dx, int *dy);
