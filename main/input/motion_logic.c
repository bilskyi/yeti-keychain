#include "input/motion_logic.h"

#include <stdlib.h>

void motion_init(motion_t *m) {
    *m = (motion_t){0};
}

static bool before(uint32_t now, bool set, uint32_t until) {
    return set && (int32_t)(until - now) > 0;
}

static input_event_t update_shake(motion_t *m, uint32_t now) {
    m->hits[m->next] = now;
    m->next = (m->next + 1) % ACCEL_SHAKE_HITS;
    if (m->nhits < ACCEL_SHAKE_HITS) m->nhits++;
    if (m->nhits < ACCEL_SHAKE_HITS) return INPUT_NONE;
    // m->next now points at the oldest of the last ACCEL_SHAKE_HITS hits.
    if ((int32_t)(now - m->hits[m->next]) > ACCEL_SHAKE_WINDOW_MS) return INPUT_NONE;
    m->nhits = 0;
    m->quiet = true;
    m->quiet_until = now + ACCEL_SHAKE_COOLDOWN_MS;
    m->candidate = m->tilt;  // a tilt seen mid-shake is not a tilt
    return INPUT_SHAKE;
}

static input_event_t update_tilt(motion_t *m, int t, uint32_t now) {
    tilt_t want = m->tilt;
    if (t <= -ACCEL_TILT_ENTER_MG) {
        want = TILT_LEFT;
    } else if (t >= ACCEL_TILT_ENTER_MG) {
        want = TILT_RIGHT;
    } else if (abs(t) <= ACCEL_TILT_EXIT_MG) {
        want = TILT_LEVEL;
    }  // in between: hysteresis, keep what we have

    if (want != m->candidate) {
        m->candidate = want;
        m->candidate_since = now;
    }
    if (m->candidate == m->tilt || (int32_t)(now - m->candidate_since) < ACCEL_TILT_HOLD_MS) {
        return INPUT_NONE;
    }
    m->tilt = m->candidate;
    if (m->tilt == TILT_LEFT) return INPUT_TILT_LEFT;
    if (m->tilt == TILT_RIGHT) return INPUT_TILT_RIGHT;
    return INPUT_NONE;  // back to level is not an event
}

input_event_t motion_update(motion_t *m, int x_mg, int y_mg, int z_mg, uint32_t now) {
    // Compare squared magnitudes against squared bounds: no sqrt, no floats.
    const int64_t mag2 = (int64_t)x_mg * x_mg + (int64_t)y_mg * y_mg + (int64_t)z_mg * z_mg;
    const int64_t hi = 1000 + ACCEL_SHAKE_DEV_MG, lo = 1000 - ACCEL_SHAKE_DEV_MG;
    const int64_t ahi = 1000 + ACCEL_ACTIVE_DEV_MG, alo = 1000 - ACCEL_ACTIVE_DEV_MG;
    const bool moving = mag2 > ahi * ahi || mag2 < alo * alo;

    if (moving) {
        m->active = true;
        m->active_until = now + 1000;
    }
    if (before(now, m->quiet, m->quiet_until)) return INPUT_NONE;
    m->quiet = false;

    if (mag2 > hi * hi || mag2 < lo * lo) return update_shake(m, now);
    if (moving) return INPUT_NONE;  // gravity is not a reliable level while moving

    const int axis[3] = {x_mg, y_mg, z_mg};
    return update_tilt(m, ACCEL_TILT_SIGN * axis[ACCEL_TILT_AXIS], now);
}

input_event_t motion_click(motion_t *m, uint32_t now) {
    if (!motion_tap_allowed(m, now)) {
        m->tap_pending = false;  // a shake swallows a half-finished tap too
        return INPUT_NONE;
    }
    // Ringing: each click less than ACCEL_TAP_RING_MS after the previous one
    // (accepted or not) extends the same tap. A knock-style double tap has a
    // longer quiet gap than that before its second tap.
    const bool ringing = m->tap_seen && (int32_t)(now - m->tap_last) < ACCEL_TAP_RING_MS;
    m->tap_seen = true;
    m->tap_last = now;
    if (ringing) return INPUT_NONE;
    if (!m->tap_pending) {
        m->tap_pending = true;
        m->tap_first = now;
        return INPUT_NONE;
    }
    m->tap_pending = false;
    return INPUT_DOUBLE_TAP;
}

input_event_t motion_tap_poll(motion_t *m, uint32_t now) {
    if (!m->tap_pending || (int32_t)(now - m->tap_first) < ACCEL_TAP_DOUBLE_MS) return INPUT_NONE;
    m->tap_pending = false;
    return INPUT_TAP;
}

bool motion_busy(const motion_t *m, uint32_t now) {
    return before(now, m->active, m->active_until) || before(now, m->quiet, m->quiet_until) ||
           m->candidate != m->tilt || m->tap_pending;
}

bool motion_tap_allowed(const motion_t *m, uint32_t now) {
    if (before(now, m->quiet, m->quiet_until)) return false;
    // One hit can be the tap itself; two in half a second is a shake starting.
    int recent = 0;
    for (int i = 0; i < m->nhits; i++) {
        const uint32_t t = m->hits[(m->next - 1 - i + ACCEL_SHAKE_HITS) % ACCEL_SHAKE_HITS];
        if ((int32_t)(now - t) <= 500) recent++;
    }
    return recent < 2;
}
