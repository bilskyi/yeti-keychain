// The Yeti's own life between interactions, and its backstory: it is YETI-7,
// a little expedition robot that lost its base and now lives in a keychain,
// held together with tape. It
//   - sounds an alarm (siren, red LED) when it starts needing something,
//     and again every so often while the need lasts;
//   - glitches now and then: the screen tears, it chirps garbage and shows a
//     fragment of its log ("SIGNAL LOST", "CORE TEMP LOW"...);
//   - bursts into song at random; shake it to make it stop;
//   - hangs now and then ("NOT RESPONDING"): shake it to bring it back.
//
// Pure C (host-tested). main.c does the sound, LED and face for each event.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "face/rng.h"

typedef enum {
    STORY_NONE = 0,
    STORY_ALARM,
    STORY_GLITCH,
    STORY_SONG,
    STORY_HANG,
} story_event_t;

typedef struct {
    rng_t rng;
    uint32_t next_glitch, next_song, next_alarm, next_hang;
    bool alarm_due;        // a need is waiting to be announced
    uint32_t quiet_until;  // events are spaced at least STORY_GAP_MS apart
    uint8_t needs;         // as of the last update
    int song;              // the song the last STORY_SONG should play
    int log;               // next log line
} story_t;

#define STORY_GAP_MS (2UL * 60UL * 1000UL)

void story_init(story_t *s, uint32_t seed, uint32_t now);

// Every frame. `idle` = awake, the face on screen and nothing else going on;
// an event that comes due while busy waits until idle. `can_sing` = sound
// on. Returns the event to start now, if any.
story_event_t story_update(story_t *s, uint32_t now, uint8_t needs, bool idle, bool can_sing);

int story_song(const story_t *s);  // for STORY_SONG
// The log line to show with a STORY_GLITCH (advances through the story).
const char *story_log(story_t *s);
