#include "power/policy.h"

#include "config.h"

void policy_init(policy_t *p) {
    *p = (policy_t){0};
}

// Night-mode stub: night is the tail of a repeating uptime cycle until there
// is a real clock (see config.h).
static bool scheduled_night(uint64_t now_ms, uint64_t *cycle) {
    const uint64_t len = (uint64_t)NIGHT_CYCLE_AWAKE_MS + NIGHT_CYCLE_NIGHT_MS;
    if (NIGHT_CYCLE_NIGHT_MS == 0 || len == 0) {
        *cycle = 0;
        return false;
    }
    *cycle = now_ms / len;
    return now_ms % len >= NIGHT_CYCLE_AWAKE_MS;
}

void policy_on_event(policy_t *p, input_event_t ev, uint64_t now_ms) {
    uint64_t cycle;
    const bool sched = scheduled_night(now_ms, &cycle);

    if (ev == INPUT_BTN_A_LONG && !p->night) {
        p->forced = true;
        return;
    }
    // Anything else (or BTN_A_LONG while asleep) wakes up.
    p->forced = false;
    if (sched) {
        p->woken = true;
        p->woken_cycle = cycle;
    }
}

bool policy_update(policy_t *p, uint64_t now_ms) {
    uint64_t cycle;
    const bool sched = scheduled_night(now_ms, &cycle);
    if (p->woken && cycle != p->woken_cycle) p->woken = false;

    const bool night = p->forced || (sched && !p->woken);
    const bool changed = night != p->night;
    p->night = night;
    return changed;
}

uint8_t policy_contrast(const policy_t *p) {
    return p->night ? CONTRAST_NIGHT : CONTRAST_DAY;
}

void policy_burnin_offset(uint64_t now_ms, int *dx, int *dy) {
    // A loop around the origin that never repeats a neighbour's position
    // twice in a row, so no edge pixel stays lit across two steps.
    static const int8_t LOOP[][2] = {
        {0, 0}, {1, 0}, {2, 1}, {1, 2}, {0, 1}, {-1, 2}, {-2, 1}, {-1, 0},
        {-2, -1}, {-1, -2}, {0, -1}, {1, -2}, {2, -1},
    };
    const unsigned n = sizeof(LOOP) / sizeof(LOOP[0]);
    const unsigned i = (unsigned)((now_ms / BURNIN_SHIFT_MS) % n);
    int x = LOOP[i][0], y = LOOP[i][1];
    if (x > BURNIN_MAX_SHIFT_PX) x = BURNIN_MAX_SHIFT_PX;
    if (x < -BURNIN_MAX_SHIFT_PX) x = -BURNIN_MAX_SHIFT_PX;
    if (y > BURNIN_MAX_SHIFT_PX) y = BURNIN_MAX_SHIFT_PX;
    if (y < -BURNIN_MAX_SHIFT_PX) y = -BURNIN_MAX_SHIFT_PX;
    *dx = x;
    *dy = y;
}
