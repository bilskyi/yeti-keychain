// The pet's life: how fed, entertained and rested it is, and how far it has
// grown. Ticks once per minute of uptime; caring for it (food, games,
// petting) tops the stats up. The face reads the needs and sulks when one
// runs low.
//
// Pure C, no ESP-IDF (host-tested). Time only exists while powered: with no
// clock across power-off, the pet simply pauses.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PET_MAX 100
#define PET_LOW 25  // below this a stat becomes a need

// Need bits, as the face takes them.
#define PET_NEED_FOOD 0x01
#define PET_NEED_FUN 0x02
#define PET_NEED_SLEEP 0x04

typedef struct {
    uint8_t food, fun, energy;  // 0..PET_MAX
    uint8_t level;              // 1.., 0 = never initialised
    uint16_t xp;                // toward the next level
    uint32_t minutes;           // age, in powered minutes
} pet_t;

void pet_init(pet_t *p);

// One minute passes. Hunger always grows, boredom only while awake; energy
// drains awake and comes back asleep.
void pet_minute(pet_t *p, bool asleep);

// Eat something worth `food` (and `fun`). False when it is too full to eat.
bool pet_feed(pet_t *p, int food, int fun);

// Adds fun (may be negative) and costs energy.
void pet_play(pet_t *p, int fun, int energy);

// Adds experience. True when that made it level up.
bool pet_add_xp(pet_t *p, int xp);
int pet_xp_next(int level);  // xp needed to leave this level

uint8_t pet_needs(const pet_t *p);  // PET_NEED_* bits
