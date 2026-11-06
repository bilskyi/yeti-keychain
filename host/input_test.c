// Host unit tests for the input logic: button debounce / short / long, and
// shake / tilt from accelerometer samples, tap / double tap from click flags,
// touch pad gestures, and what the microphone hears.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "input/button_logic.h"
#include "input/motion_logic.h"
#include "input/mic_logic.h"
#include "input/touch_logic.h"

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

    // Shaking on and on: another SHAKE every ~0.8 s, so 4 s make about five.
    motion_init(&m);
    shakes = 0;
    for (int i = 0; i < 4000 / FRAME; i++) {
        const int x = (i % 2) ? 2500 : -1500;
        shakes += motion_update(&m, x, 0, 1000, now += FRAME) == INPUT_SHAKE;
    }
    printf("  shakes in 4 s of shaking: %d\n", shakes);
    assert(shakes >= 4 && shakes <= 6);
    assert(!motion_tap_allowed(&m, now));

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

// Holds the pad touched (or not) for ms, sampling every FRAME; counts ev.
static int touch_for(touch_t *t, bool on, uint32_t ms, uint32_t *now, input_event_t ev, int *other) {
    int n = 0;
    for (uint32_t s = 0; s < ms; s += FRAME) {
        const input_event_t e = touch_update(t, on, *now += FRAME);
        if (e == ev) {
            n++;
        } else if (e != INPUT_NONE) {
            (*other)++;
        }
    }
    return n;
}

static void test_touch(void) {
    touch_t t;
    uint32_t now = 1000;
    int other = 0;
    touch_init(&t);
    assert(!touch_busy(&t));

    // A pat: one TOUCH on release.
    assert(touch_for(&t, true, 200, &now, INPUT_TOUCH, &other) == 0);
    assert(touch_busy(&t));
    assert(touch_for(&t, false, 300, &now, INPUT_TOUCH, &other) == 1 && other == 0);
    assert(!touch_busy(&t));

    // Petting: HOLD while still held, nothing more while held, END on release.
    now += 3000;
    assert(touch_for(&t, true, 3000, &now, INPUT_TOUCH_HOLD, &other) == 1 && other == 0);
    assert(touch_busy(&t));
    assert(touch_for(&t, false, 300, &now, INPUT_TOUCH_END, &other) == 1 && other == 0);
    assert(!touch_busy(&t));

    // Three quick pats: TOUCH, TOUCH, TICKLE.
    now += 3000;
    int touches = 0, tickles = 0;
    for (int i = 0; i < 3; i++) {
        touch_for(&t, true, 120, &now, INPUT_NONE, &other);
        touches += touch_for(&t, false, 200, &now, INPUT_TOUCH, &tickles);
    }
    assert(touches == 2 && tickles == 1);
    other = 0;

    // Three slow pats: three TOUCHes, no tickle.
    now += 3000;
    touches = 0;
    for (int i = 0; i < 3; i++) {
        touch_for(&t, true, 120, &now, INPUT_NONE, &other);
        touches += touch_for(&t, false, 900, &now, INPUT_TOUCH, &other);
    }
    assert(touches == 3 && other == 0);

    // A 10 ms glitch is not a touch.
    assert(touch_update(&t, true, now += 10) == INPUT_NONE);
    assert(touch_for(&t, false, 300, &now, INPUT_TOUCH, &other) == 0 && other == 0);
}

// --- microphone: synthetic 10 ms blocks at a mid-rail bias ---

static uint32_t s_lcg = 1;

// kind: 0 = quiet hiss, 1 = 500 Hz square of amplitude `amp`, 2 = overloaded (blowing).
static void make_block(uint16_t *b, int kind, int amp) {
    for (int i = 0; i < MIC_BLOCK; i++) {
        s_lcg = s_lcg * 1103515245u + 12345u;
        const int hiss = (int)((s_lcg >> 16) % 17) - 8;
        int v = 2048 + hiss;
        if (kind == 1) v += (i / 8) % 2 ? amp : -amp;
        if (kind == 2) v = (i / 6) % 2 ? 4095 : 0;
        b[i] = (uint16_t)(v < 0 ? 0 : v > 4095 ? 4095 : v);
    }
}

typedef struct {
    int n[INPUT_EVENT_COUNT];
} heard_t;

static void hear(mic_t *m, int kind, int amp, uint32_t ms, uint32_t *now, heard_t *h) {
    uint16_t b[MIC_BLOCK];
    for (uint32_t t = 0; t < ms; t += 10) {
        make_block(b, kind, amp);
        h->n[mic_block(m, b, MIC_BLOCK, *now += 10)]++;
        h->n[mic_poll(m, *now)]++;
    }
}

static int total(const heard_t *h) {
    int n = 0;
    for (int i = 1; i < INPUT_EVENT_COUNT; i++) n += h->n[i];
    return n;
}

static void test_mic_loudness(void) {
    static mic_t m;
    uint32_t now = 5000;
    heard_t h = {0};
    mic_init(&m);
    hear(&m, 0, 0, 5000, &now, &h);  // a quiet room
    assert(mic_loudness(&m) == 0);
    // Louder sounds read higher, a shout near the top; it falls back after.
    int prev = 0;
    for (int amp = 120; amp <= 600; amp += 160) {
        hear(&m, 1, amp, 100, &now, &h);
        const int l = mic_loudness(&m);
        printf("  amplitude %d: loudness %d\n", amp, l);
        assert(l > prev && l <= 100);
        prev = l;
    }
    assert(prev >= 80);
    hear(&m, 0, 0, 500, &now, &h);
    assert(mic_loudness(&m) == 0);
    // Deaf (our own buzzer): 0, however loud.
    mic_deaf(&m, now + 1000);
    hear(&m, 1, 600, 200, &now, &h);
    assert(mic_loudness(&m) == 0);
}

static void test_mic(void) {
    static mic_t m;
    uint32_t now = 5000;
    heard_t h = {0};
    mic_init(&m);

    // A quiet room: nothing, for a long time.
    hear(&m, 0, 0, 20000, &now, &h);
    assert(total(&h) == 0);

    // One clap: a CLAP, but only once the double-clap window has passed.
    hear(&m, 1, 900, 30, &now, &h);
    hear(&m, 0, 0, 400, &now, &h);
    assert(total(&h) == 0);
    hear(&m, 0, 0, 800, &now, &h);
    assert(total(&h) == 1 && h.n[INPUT_CLAP] == 1);
    mic_sound_t s;
    assert(mic_take_sound(&m, &s) && s.ev == INPUT_CLAP && s.ms <= MIC_CLAP_MAX_MS);

    // Clap-clap: a DOUBLE_CLAP and no single CLAP.
    h = (heard_t){0};
    hear(&m, 1, 900, 30, &now, &h);
    hear(&m, 0, 0, 300, &now, &h);
    hear(&m, 1, 900, 30, &now, &h);
    hear(&m, 0, 0, 1500, &now, &h);
    assert(total(&h) == 1 && h.n[INPUT_DOUBLE_CLAP] == 1);

    // Two claps a second apart: two CLAPs.
    h = (heard_t){0};
    hear(&m, 1, 900, 30, &now, &h);
    hear(&m, 0, 0, 1000, &now, &h);
    hear(&m, 1, 900, 30, &now, &h);
    hear(&m, 0, 0, 1500, &now, &h);
    assert(total(&h) == 2 && h.n[INPUT_CLAP] == 2);

    // Talking: four syllables with short gaps. HEAR once it goes on, then
    // TALK at the end, with the syllables counted.
    h = (heard_t){0};
    for (int i = 0; i < 4; i++) {
        hear(&m, 1, 150, 160, &now, &h);
        hear(&m, 0, 0, 80, &now, &h);
    }
    assert(h.n[INPUT_HEAR] == 1 && h.n[INPUT_TALK] == 0);
    hear(&m, 0, 0, 500, &now, &h);
    assert(total(&h) == 2 && h.n[INPUT_TALK] == 1);
    assert(mic_syllables(&m) == 4);

    // A shout: LOUD, and no listening face first.
    h = (heard_t){0};
    hear(&m, 1, 800, 600, &now, &h);
    hear(&m, 0, 0, 500, &now, &h);
    assert(total(&h) == 1 && h.n[INPUT_LOUD] == 1);

    // Blowing on it overloads the mic: BLOW.
    h = (heard_t){0};
    hear(&m, 2, 0, 700, &now, &h);
    hear(&m, 0, 0, 500, &now, &h);
    assert(total(&h) == 1 && h.n[INPUT_BLOW] == 1);

    // A short soft knock is nothing.
    h = (heard_t){0};
    hear(&m, 1, 120, 50, &now, &h);
    hear(&m, 0, 0, 1500, &now, &h);
    assert(total(&h) == 0);

    // Music that goes on and on: maybe a HEAR at the start, never a TALK, and
    // after a while it is just the room.
    h = (heard_t){0};
    hear(&m, 1, 150, 20000, &now, &h);
    assert(h.n[INPUT_TALK] == 0 && h.n[INPUT_LOUD] == 0 && h.n[INPUT_HEAR] <= 1);
    h = (heard_t){0};
    hear(&m, 1, 150, 5000, &now, &h);
    assert(total(&h) == 0);
    hear(&m, 0, 0, 5000, &now, &h);  // quiet again: the floor drops back

    // Deaf (our own buzzer): a clap meanwhile is nothing; after, it counts.
    h = (heard_t){0};
    mic_deaf(&m, now + 500);
    hear(&m, 1, 900, 30, &now, &h);
    hear(&m, 0, 0, 1500, &now, &h);
    assert(total(&h) == 0);
    hear(&m, 1, 900, 30, &now, &h);
    hear(&m, 0, 0, 1500, &now, &h);
    assert(total(&h) == 1 && h.n[INPUT_CLAP] == 1);

    // A steady hum below MIC_MIN_LEVEL still raises the floor, slowly: then
    // soft speech is lost in it, where it was heard in the quiet.
    h = (heard_t){0};
    hear(&m, 1, 28, 30000, &now, &h);
    assert(total(&h) == 0);
    hear(&m, 1, 75, 600, &now, &h);
    hear(&m, 1, 28, 500, &now, &h);
    assert(total(&h) == 0);
    hear(&m, 0, 0, 5000, &now, &h);
    hear(&m, 1, 75, 600, &now, &h);
    hear(&m, 0, 0, 500, &now, &h);
    assert(h.n[INPUT_TALK] == 1);

    // A noisy room (a fan) sets a higher floor: speech at that level is not
    // a sound, louder speech still is.
    h = (heard_t){0};
    hear(&m, 1, 60, 15000, &now, &h);
    assert(h.n[INPUT_TALK] == 0);
    h = (heard_t){0};
    hear(&m, 1, 70, 800, &now, &h);
    hear(&m, 1, 60, 500, &now, &h);
    assert(total(&h) == 0);
    hear(&m, 1, 400, 800, &now, &h);
    hear(&m, 1, 60, 500, &now, &h);
    assert(h.n[INPUT_TALK] == 1);
}

int main(void) {
#define RUN(t) (printf("%s\n", #t), t())
    RUN(test_touch);
    RUN(test_mic);
    RUN(test_mic_loudness);
    RUN(test_button);
    RUN(test_motion_still);
    RUN(test_motion_shake);
    RUN(test_motion_tilt);
    RUN(test_taps);
    printf("all input tests passed\n");
    return 0;
}
