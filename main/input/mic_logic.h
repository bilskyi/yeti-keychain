// What the microphone heard, with no hardware in it: claps, a double clap, a
// shout, blowing on it, and someone talking (with a rough syllable count).
// Pure C (host-tested). mic.c feeds it 10 ms blocks of raw ADC samples.
//
// Each block becomes one level (mean distance from the DC bias). A slowly
// adapting noise floor sets the threshold, so a loud room does not count as
// one endless sound. A run of loud blocks is a "sound"; when it ends (after
// MIC_END_GAP_MS of quiet) its length, peak and overload decide what it was.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "input/input_event.h"

// One finished sound, for the log (tuning the thresholds on the bench).
typedef struct {
    uint16_t ms;
    uint16_t peak, mean, floor;
    uint8_t clip_pct;
    uint8_t syllables;
    input_event_t ev;  // what it was taken for (INPUT_NONE: nothing)
} mic_sound_t;

typedef struct {
    int32_t dc;     // DC bias, x256
    int32_t floor;  // noise floor level, x16
    bool primed;    // dc has been seeded from the first block

    // The sound in progress.
    bool in_sound;
    uint32_t start, last_loud;
    int peak;
    int32_t sum;
    int blocks, clip_blocks;
    int syllables;
    bool syl_up;  // inside a syllable (above threshold)
    bool heard;   // INPUT_HEAR went out for this one

    // A clap waiting to see whether a second one follows.
    bool clap_pending;
    uint32_t clap_at;

    uint32_t deaf_until;
    bool deaf;

    mic_sound_t last;
    bool has_last;
} mic_t;

void mic_init(mic_t *m);

// Ignore everything until `until` (our own buzzer). A sound in progress is
// dropped.
void mic_deaf(mic_t *m, uint32_t until);

// One block of raw 12-bit samples ending at `now`. Returns INPUT_HEAR,
// INPUT_TALK, INPUT_LOUD, INPUT_BLOW, INPUT_DOUBLE_CLAP or INPUT_NONE.
input_event_t mic_block(mic_t *m, const uint16_t *s, int n, uint32_t now);

// Every block (or so): INPUT_CLAP once a lone clap's double window passed.
input_event_t mic_poll(mic_t *m, uint32_t now);

// The last finished sound, once (for logging).
bool mic_take_sound(mic_t *m, mic_sound_t *out);

// Syllables in the last INPUT_TALK, 1-9.
int mic_syllables(const mic_t *m);
