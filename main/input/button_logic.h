// One button's debounce and short/long classification, with no hardware in
// it. Pure C (host-tested). buttons.c samples the pin and calls this.

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BUTTON_NONE = 0,
    BUTTON_SHORT,  // released before BTN_LONG_MS
    BUTTON_LONG,   // held for BTN_LONG_MS; fires once, while still held
} button_result_t;

typedef struct {
    bool raw;            // last sample
    bool stable;         // debounced state (true = pressed)
    uint32_t raw_since;  // when raw last changed
    uint32_t pressed_at;
    bool long_sent;
} button_t;

void button_init(button_t *b);

// Feed one sample. A change counts once it has held for BTN_DEBOUNCE_MS.
button_result_t button_update(button_t *b, bool pressed, uint32_t now);

// True while pressed or still settling: the caller should keep sampling fast.
bool button_busy(const button_t *b);
