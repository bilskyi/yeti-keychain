// Host unit tests for the input logic: button debounce / short / long, and
// shake / tilt from accelerometer samples, tap / double tap from click flags.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "input/button_logic.h"
#include "input/motion_logic.h"

#define FRAME 33

// Holds `pressed` for `ms`, sampling every FRAME; returns the last non-NONE result.
static button_result_t hold(button_t *b, bool pressed, uint32_t ms, uint32_t *now) {
    button_result_t got = BUTTON_NONE;
    for (uint32_t t = 0; t < ms; t += FRAME) {
        const button_result_t r = button_update(b, pressed, *now += FRAME);
        if (r == BUTTON_PRESS) {
            assert(pressed);  // only ever on the way down
            continue;
        }
        if (r != BUTTON_NONE) {
            assert(got == BUTTON_NONE);  // at most one result per hold
            got = r;
        }
    }
    return got;
}

static void test_button(void) {
    button_t b;
    uint32_t now = 1000;
    button_init(&b);
    assert(!button_busy(&b));

    // PRESS comes once, as soon as the press has settled.
    int presses = 0;
    for (int i = 0; i < 10; i++) presses += button_update(&b, true, now += FRAME) == BUTTON_PRESS;
    assert(presses == 1);
    assert(hold(&b, false, 100, &now) == BUTTON_SHORT);

    // Short press.
    assert(hold(&b, true, 150, &now) == BUTTON_NONE);
    assert(button_busy(&b));
    assert(hold(&b, false, 100, &now) == BUTTON_SHORT);
    assert(!button_busy(&b));

    // Long press fires while held, and nothing on release.
    assert(hold(&b, true, BTN_LONG_MS + 100, &now) == BUTTON_LONG);
    assert(hold(&b, false, 100, &now) == BUTTON_NONE);

    // A one-sample glitch (shorter than the debounce) is not a press.
    button_init(&b);
    assert(button_update(&b, true, now += 10) == BUTTON_NONE);
    assert(button_update(&b, false, now += 10) == BUTTON_NONE);
    assert(hold(&b, false, 200, &now) == BUTTON_NONE);
    assert(!b.stable);

    // Bounce on release does not give two presses.
    assert(hold(&b, true, 150, &now) == BUTTON_NONE);
    int shorts = 0;
    const bool bounce[] = {false, true, false, true, false, false, false, false};
    for (unsigned i = 0; i < sizeof(bounce); i++) {
        const button_result_t r = button_update(&b, bounce[i], now += 5);
        shorts += r == BUTTON_SHORT;
        assert(r != BUTTON_PRESS);  // bounce is never a second press
    }
    shorts += hold(&b, false, 100, &now) == BUTTON_SHORT;
    assert(shorts == 1);

    // Survives the 49-day wrap.
    button_init(&b);
    now = UINT32_MAX - 100;
    assert(hold(&b, true, 150, &now) == BUTTON_NONE);
    assert(hold(&b, false, 100, &now) == BUTTON_SHORT);
}

// Feeds the same sample for `ms`; counts how many times `want` came out.
static int feed(motion_t *m, int x, int y, int z, uint32_t ms, uint32_t *now, input_event_t want) {
    int n = 0;
    for (uint32_t t = 0; t < ms; t += FRAME) n += motion_update(m, x, y, z, *now += FRAME) == want;
    return n;
}

static void test_motion_still(void) {
    motion_t m;
    uint32_t now = 1000;
    motion_init(&m);
    // Flat on a desk: nothing, ever, and back to slow polling.
    assert(feed(&m, 0, 0, 1000, 5000, &now, INPUT_NONE) > 0);
    assert(feed(&m, 0, 0, 1000, 5000, &now, INPUT_SHAKE) == 0);
    assert(!motion_busy(&m, now));
    assert(motion_tap_allowed(&m, now));
}

static void test_motion_shake(void) {
    motion_t m;
    uint32_t now = 1000;
    motion_init(&m);
    // Shaking: alternate +2.5 g / -0.5 g swings at ~30 Hz.
    int shakes = 0;
    for (int i = 0; i < 20; i++) {
        const int x = (i % 2) ? 2500 : -1500;
        shakes += motion_update(&m, x, 0, 1000, now += FRAME) == INPUT_SHAKE;
    }
    assert(shakes == 1);  // one event per shake, not one per sample
    assert(!motion_tap_allowed(&m, now));  // the click unit fires all through a shake
    assert(motion_busy(&m, now));
    // After the cooldown, still and taps count again.
    feed(&m, 0, 0, 1000, ACCEL_SHAKE_COOLDOWN_MS + 1100, &now, INPUT_NONE);
    assert(motion_tap_allowed(&m, now));
    assert(!motion_busy(&m, now));

    // A single bump (a tap) is not a shake, and the tap itself still counts.
    motion_init(&m);
    assert(motion_update(&m, 0, 0, 2500, now += FRAME) == INPUT_NONE);
    assert(motion_tap_allowed(&m, now));
    assert(feed(&m, 0, 0, 1000, 3000, &now, INPUT_SHAKE) == 0);

    // Big swings spread out over seconds are not a shake either.
    motion_init(&m);
    for (int i = 0; i < 8; i++) {
        assert(motion_update(&m, 2500, 0, 1000, now += 600) == INPUT_NONE);
    }
}

static void test_motion_tilt(void) {
    motion_t m;
    uint32_t now = 1000;
    motion_init(&m);
    const int left = -ACCEL_TILT_SIGN * 700, right = ACCEL_TILT_SIGN * 700;
    int axis[3] = {0, 0, 0};

    // Tilted ~45 degrees left: one event after the hold time, not before.
    axis[ACCEL_TILT_AXIS] = left;
    axis[(ACCEL_TILT_AXIS + 2) % 3] = 700;
    assert(motion_update(&m, axis[0], axis[1], axis[2], now += FRAME) == INPUT_NONE);
    assert(motion_busy(&m, now));  // confirming: sample fast
    assert(feed(&m, axis[0], axis[1], axis[2], 1000, &now, INPUT_TILT_LEFT) == 1);
    assert(!motion_busy(&m, now));

    // Hysteresis: easing back to ~20 degrees is still left, no new event.
    axis[ACCEL_TILT_AXIS] = left / 2;
    axis[(ACCEL_TILT_AXIS + 2) % 3] = 940;
    assert(feed(&m, axis[0], axis[1], axis[2], 1000, &now, INPUT_TILT_LEFT) == 0);
    assert(m.tilt == TILT_LEFT);

    // Level, then right.
    axis[ACCEL_TILT_AXIS] = 0;
    axis[(ACCEL_TILT_AXIS + 2) % 3] = 1000;
    feed(&m, axis[0], axis[1], axis[2], 1000, &now, INPUT_NONE);
    assert(m.tilt == TILT_LEVEL);
    axis[ACCEL_TILT_AXIS] = right;
    axis[(ACCEL_TILT_AXIS + 2) % 3] = 700;
    assert(feed(&m, axis[0], axis[1], axis[2], 1000, &now, INPUT_TILT_RIGHT) == 1);

    // A brief tilt (shorter than the hold) is ignored.
    motion_init(&m);
    axis[ACCEL_TILT_AXIS] = left;
    axis[(ACCEL_TILT_AXIS + 2) % 3] = 700;
    assert(feed(&m, axis[0], axis[1], axis[2], ACCEL_TILT_HOLD_MS - 100, &now, INPUT_TILT_LEFT) == 0);
    axis[ACCEL_TILT_AXIS] = 0;
    axis[(ACCEL_TILT_AXIS + 2) % 3] = 1000;
    assert(feed(&m, axis[0], axis[1], axis[2], 1000, &now, INPUT_TILT_LEFT) == 0);
}

// One physical tap as the bench saw it: the click unit fires again while the
// board rings, 30-60 ms apart.
static input_event_t ringing_tap(motion_t *m, uint32_t *now) {
    input_event_t got = INPUT_NONE;
    const uint32_t gaps[] = {0, 30, 60, 30};
    for (unsigned i = 0; i < sizeof(gaps) / sizeof(gaps[0]); i++) {
        const input_event_t ev = motion_click(m, *now += gaps[i]);
        if (ev != INPUT_NONE) got = ev;
    }
    return got;
}

// Runs frames with no clicks; counts `want` from motion_tap_poll.
static int idle(motion_t *m, uint32_t ms, uint32_t *now, input_event_t want) {
    int n = 0;
    for (uint32_t t = 0; t < ms; t += FRAME) n += motion_tap_poll(m, *now += FRAME) == want;
    return n;
}

static void test_taps(void) {
    motion_t m;
    uint32_t now = 1000;
    motion_init(&m);

    // A ringing single tap is one TAP, after the double window, not four.
    assert(ringing_tap(&m, &now) == INPUT_NONE);
    assert(motion_busy(&m, now));  // window open: keep polling fast
    assert(idle(&m, 1000, &now, INPUT_TAP) == 1);
    assert(!motion_busy(&m, now));

    // Two ringing taps 250 ms apart: one DOUBLE_TAP and no TAP after it.
    assert(ringing_tap(&m, &now) == INPUT_NONE);
    now += 250 - 120;
    assert(ringing_tap(&m, &now) == INPUT_DOUBLE_TAP);
    assert(idle(&m, 1000, &now, INPUT_TAP) == 0);

    // A fast knock: second tap 130 ms after the first's ringing started, with
    // a 100 ms quiet gap, is still a double (the bench missed these at a fixed
    // 150 ms dead time).
    assert(motion_click(&m, now += 10) == INPUT_NONE);
    assert(motion_click(&m, now += 30) == INPUT_NONE);  // ringing
    assert(motion_click(&m, now += 100) == INPUT_DOUBLE_TAP);
    assert(idle(&m, 1000, &now, INPUT_TAP) == 0);

    // Ringing that goes on for a long time is still one tap.
    for (int i = 0; i < 10; i++) assert(motion_click(&m, now += 60) == INPUT_NONE);
    assert(idle(&m, 1000, &now, INPUT_TAP) == 1);

    // Two taps too far apart: two TAPs.
    assert(motion_click(&m, now += 10) == INPUT_NONE);
    assert(idle(&m, 700, &now, INPUT_TAP) == 1);
    assert(motion_click(&m, now += 10) == INPUT_NONE);
    assert(idle(&m, 700, &now, INPUT_TAP) == 1);

    // Clicks during a shake count for nothing.
    for (int i = 0; i < 20; i++) motion_update(&m, (i % 2) ? 2500 : -1500, 0, 1000, now += FRAME);
    assert(motion_click(&m, now += 10) == INPUT_NONE);
    assert(idle(&m, 700, &now, INPUT_TAP) == 0);
}

int main(void) {
#define RUN(t) (printf("%s\n", #t), t())
    RUN(test_button);
    RUN(test_motion_still);
    RUN(test_motion_shake);
    RUN(test_motion_tilt);
    RUN(test_taps);
    printf("all input tests passed\n");
    return 0;
}
