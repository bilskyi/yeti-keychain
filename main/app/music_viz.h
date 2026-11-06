// The music player's animations: one is picked at random for each song.
// Each is driven by the song's own notes: music.c parses the tune the
// buzzer is playing, tells the animation when each note starts, and lets it
// look ahead (Note Rain, Bouncing Ball draw the notes still to come).
//
// Sasha picked these nine in the Yeti Visualizers artifact
// (https://claude.ai/artifact/FmounxUi3W3KfGsELwoJWa, db `picks/visualizers`).
//
// Pure C (host-tested). Positions in 1/16 px, times in ms.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "display/gfx.h"
#include "face/rng.h"
#include "sound/rtttl.h"

typedef enum {
    VIZ_DANCE,      // the whole Yeti dancing under a disco ball
    VIZ_BOUNCE,     // karaoke: a ball hops along the melody's steps
    VIZ_SYNTHWAVE,  // a striped sunset over a racing grid
    VIZ_VINYL,      // a spinning record, tonearm and level meter
    VIZ_DJ,         // the Yeti's face in headphones, nodding
    VIZ_WARP,       // a starfield, a burst of speed on each note
    VIZ_RAIN,       // Guitar Hero: notes fall into five lanes
    VIZ_SCOPE,      // a wave on a scope grid
    VIZ_EQ,         // sixteen LED-segment bars
    VIZ_COUNT
} viz_kind_t;

#define VIZ_TRACK_MAX 320  // notes and rests of the longest song

// The song as notes and rests, no gaps (start times add up exactly), each
// note's place between the song's lowest (0) and highest (255) pitch.
typedef struct {
    note_t notes[VIZ_TRACK_MAX];
    uint8_t pitch[VIZ_TRACK_MAX];
    int n;
} viz_track_t;

#define VIZ_SPARKS 12
#define VIZ_STARS 48
#define VIZ_FLOATS 4

typedef struct {
    int16_t x, y, vx, vy, life;
} viz_bit_t;

typedef struct {
    viz_kind_t kind;
    int notes_seen;  // notes started so far
    union {
        struct {
            int16_t h[16], pk[16], hold[16];
        } eq;
        struct {
            int16_t flash[5];
            viz_bit_t sparks[VIZ_SPARKS];
        } rain;
        struct {
            int amp, per, ph;
        } scope;
        struct {
            int16_t x[VIZ_STARS], y[VIZ_STARS], z[VIZ_STARS];
            int v, ring;
        } warp;
        struct {
            int bob, happy, side;
            viz_bit_t notes[VIZ_FLOATS];
        } dj;
        struct {
            int angle, level;
            viz_bit_t notes[VIZ_FLOATS];
        } vinyl;
        struct {
            int z, v;
            int16_t twinkle[18];
        } synth;
        struct {
            int move, jump, rays;
        } dance;
        struct {
            int squash;
        } bounce;
    } s;
} viz_t;

// Parses song i for the animations. False on a bad tune.
bool viz_track_load(viz_track_t *tr, int song);

void viz_start(viz_t *v, viz_kind_t kind, rng_t *rng);
// A note of the track starts (index into tr->notes).
void viz_note(viz_t *v, const viz_track_t *tr, int i, rng_t *rng);
// Time moves on by dt ms.
void viz_step(viz_t *v, int dt, rng_t *rng);
// t = ms into the song.
void viz_draw(const viz_t *v, const viz_track_t *tr, uint32_t t, gfx_t *g);

const char *viz_name(viz_kind_t kind);
