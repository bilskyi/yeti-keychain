// Power policy decisions that are pure functions of time and input: night
// mode, the burn-in shift, and the contrast that goes with them.
//
// Pure C, no ESP-IDF (host-tested). power.c does the hardware side.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "input/input_event.h"

typedef struct {
    bool night;
    bool forced;          // BTN_A_LONG put us to sleep by hand
    bool woken;           // input ended the current scheduled night early
    uint64_t woken_cycle; // ...which night that was
} policy_t;

void policy_init(policy_t *p);

// Any input wakes the face for the rest of the current night window.
// BTN_A_LONG while awake forces night until the next input.
void policy_on_event(policy_t *p, input_event_t ev, uint64_t now_ms);

// Recomputes p->night. Returns true when it changed.
bool policy_update(policy_t *p, uint64_t now_ms);

uint8_t policy_contrast(const policy_t *p);

// Burn-in shift: the offset the whole face is drawn at, stepping through a
// small loop every BURNIN_SHIFT_MS. Never beyond +-BURNIN_MAX_SHIFT_PX.
void policy_burnin_offset(uint64_t now_ms, int *dx, int *dy);
