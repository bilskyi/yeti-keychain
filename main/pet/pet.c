#include "pet/pet.h"

#define FULL_AT 95  // won't eat above this

// Minutes per point of change. Hunger: full to empty in 10 h; boredom: in
// about 7 h awake; energy: a 30-minute day costs 6, a 10-minute night gives 20.
#define FOOD_EVERY 6
#define FUN_EVERY 4
#define ENERGY_EVERY 5
#define ENERGY_SLEEP 2  // gained per minute asleep

static uint8_t clamp(int v) {
    return (uint8_t)(v < 0 ? 0 : v > PET_MAX ? PET_MAX : v);
}

void pet_init(pet_t *p) {
    *p = (pet_t){.food = 80, .fun = 80, .energy = 80, .level = 1};
}

void pet_minute(pet_t *p, bool asleep) {
    p->minutes++;
    if (p->minutes % FOOD_EVERY == 0) p->food = clamp(p->food - 1);
    if (asleep) {
        p->energy = clamp(p->energy + ENERGY_SLEEP);
        return;
    }
    if (p->minutes % FUN_EVERY == 0) p->fun = clamp(p->fun - 1);
    if (p->minutes % ENERGY_EVERY == 0) p->energy = clamp(p->energy - 1);
}

bool pet_feed(pet_t *p, int food, int fun) {
    if (p->food >= FULL_AT) return false;
    p->food = clamp(p->food + food);
    p->fun = clamp(p->fun + fun);
    return true;
}

void pet_play(pet_t *p, int fun, int energy) {
    p->fun = clamp(p->fun + fun);
    p->energy = clamp(p->energy - energy);
}

int pet_xp_next(int level) {
    return 20 + 10 * level;
}

bool pet_add_xp(pet_t *p, int xp) {
    if (xp <= 0) return false;
    int total = p->xp + xp;
    bool up = false;
    while (total >= pet_xp_next(p->level) && p->level < 99) {
        total -= pet_xp_next(p->level);
        p->level++;
        up = true;
    }
    p->xp = (uint16_t)(total > 60000 ? 60000 : total);
    return up;
}

uint8_t pet_needs(const pet_t *p) {
    uint8_t n = 0;
    if (p->food < PET_LOW) n |= PET_NEED_FOOD;
    if (p->fun < PET_LOW) n |= PET_NEED_FUN;
    if (p->energy < PET_LOW) n |= PET_NEED_SLEEP;
    return n;
}
