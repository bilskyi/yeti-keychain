#include "power/policy.h"

#include "config.h"

void policy_init(policy_t *p, uint64_t now_ms) {
    *p = (policy_t){0};
    p->last_activity = now_ms;
}

void policy_note_activity(policy_t *p, uint64_t now_ms) {
    p->last_activity = now_ms;
}

void policy_on_event(policy_t *p, input_event_t ev, uint64_t now_ms) {
    (void)ev;  // any input wakes it
    policy_note_activity(p, now_ms);
}

void policy_feel(policy_t *p, const int mg[3], bool oriented, uint64_t now_ms) {
    int delta = 0;
    for (int i = 0; i < 3; i++) {
        if (p->has_prev) delta += mg[i] > p->prev_mg[i] ? mg[i] - p->prev_mg[i] : p->prev_mg[i] - mg[i];
        p->prev_mg[i] = mg[i];
    }
    const bool moved = p->has_prev && delta >= WAKE_MOVE_MG;
    p->has_prev = true;

    if (oriented && mg[2] < FACEDOWN_MG) {
        if (p->down_since == 0) p->down_since = now_ms ? now_ms : 1;
        if (now_ms - p->down_since >= FACEDOWN_HOLD_MS) p->face_down = true;
    } else if (!oriented || mg[2] > FACEDOWN_EXIT_MG) {
        if (p->face_down) policy_note_activity(p, now_ms);  // lifted: wake up
        p->face_down = false;
        p->down_since = 0;
    }
    // Picked up while dozing (not just lying there): wake up.
    if (moved && !p->face_down) policy_note_activity(p, now_ms);
}

bool policy_update(policy_t *p, uint64_t now_ms) {
    // Activity can be stamped a little after the frame's own `now`.
    const bool idle = now_ms > p->last_activity && now_ms - p->last_activity >= SLEEP_AFTER_MS;
    const bool night = p->face_down || idle;
    const bool changed = night != p->night;
    p->night = night;
    return changed;
}

bool policy_screen_off(const policy_t *p) {
    return p->face_down;
}

uint8_t policy_contrast(const policy_t *p, uint8_t day) {
    return p->night ? CONTRAST_NIGHT : day;
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
