// Host unit tests for the pure parts of the firmware. Plain asserts; a
// failure prints the line and aborts.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "display/gfx.h"
#include "face/face.h"
#include "face/hud.h"
#include "power/battery_logic.h"
#include "power/policy.h"

#define FRAME 33

static int lit(const gfx_t *g) {
    int n = 0;
    for (int y = 0; y < GFX_H; y++)
        for (int x = 0; x < GFX_W; x++) n += gfx_get_pixel(g, x, y);
    return n;
}

// Pixels lit within `m` px of any screen edge.
static int lit_in_margin(const gfx_t *g, int m) {
    int n = 0;
    for (int y = 0; y < GFX_H; y++)
        for (int x = 0; x < GFX_W; x++)
            if (x < m || y < m || x >= GFX_W - m || y >= GFX_H - m) n += gfx_get_pixel(g, x, y);
    return n;
}

// Settle on one expression with no blink or transition in progress.
static void show(face_t *f, expression_t e, uint32_t *now) {
    face_set_expression(f, e, *now, 60000);
    for (int i = 0; i < 20; i++) face_update(f, *now += FRAME);
    f->blinking = false;
    f->next_blink = *now + 100000;
    f->next_idle = *now + 100000;
    assert(f->expr == e);
}

static void test_gfx(void) {
    static gfx_t g;
    gfx_clear(&g);
    gfx_fill_rect(&g, -5, -5, 10, 10, GFX_WHITE);  // clipped, must not crash
    assert(lit(&g) == 25);
    gfx_clear(&g);
    gfx_fill_rrect(&g, 10, 10, 20, 20, 5, GFX_WHITE);
    assert(gfx_get_pixel(&g, 10, 10) == 0);  // corner rounded off
    assert(gfx_get_pixel(&g, 20, 20) == 1);
    gfx_clear(&g);
    gfx_set_offset(&g, 2, 1);
    gfx_pixel(&g, 0, 0, GFX_WHITE);
    assert(gfx_get_pixel(&g, 2, 1) == 1);
    for (int v = 0; v < 5000; v++) {
        const int r = gfx_isqrt(v);
        assert(r * r <= v && (r + 1) * (r + 1) > v);
    }
}

static void test_every_expression_fits(void) {
    // Worst case: every expression, eyes glanced fully to each side, must
    // leave the burn-in margin dark so the shift never clips the face.
    static face_t f;
    static gfx_t g;
    uint32_t now = 1000;
    face_init(&f, 1, now);
    for (int e = 0; e < EXPR_COUNT; e++) {
        show(&f, (expression_t)e, &now);
        for (int lx = -12; lx <= 12; lx += 24) {
            f.look_x = f.look_tx = lx;
            f.look_y = f.look_ty = -3;
            gfx_clear(&g);
            face_render(&f, &g);
            if (e != EXPR_LOWBAT) assert(lit(&g) > 200);
            assert(lit_in_margin(&g, BURNIN_MAX_SHIFT_PX) == 0);
        }
        f.look_x = f.look_tx = f.look_y = f.look_ty = 0;
    }
}

static void test_blinks_at_random_intervals(void) {
    static face_t f;
    face_init(&f, 42, 0);
    face_set_expression(&f, EXPR_NEUTRAL, 0, UINT32_MAX / 2);  // hold mood still
    int blinks = 0;
    bool was = false;
    uint32_t last = 0, min_gap = UINT32_MAX, max_gap = 0;
    for (uint32_t now = 0; now < 120000; now += FRAME) {
        face_update(&f, now);
        if (f.blinking && !was) {
            if (blinks) {
                const uint32_t gap = now - last;
                if (gap > BLINK_DURATION_MS + FRAME) {  // ignore double blinks
                    if (gap < min_gap) min_gap = gap;
                    if (gap > max_gap) max_gap = gap;
                }
            }
            blinks++;
            last = now;
        }
        was = f.blinking;
    }
    printf("  blinks in 2 min: %d, gaps %u..%u ms\n", blinks, (unsigned)min_gap,
           (unsigned)max_gap);
    assert(blinks >= 120000 / (BLINK_MAX_MS + BLINK_DURATION_MS));
    assert(min_gap >= BLINK_MIN_MS);
    assert(max_gap <= BLINK_MAX_MS + BLINK_DURATION_MS + 2 * FRAME);
}

static void test_mood_machine_visits_every_mood(void) {
    static face_t f;
    face_init(&f, 7, 0);
    int seen[EXPR_COUNT] = {0};
    int yawns = 0, glances = 0;
    idle_kind_t prev_idle = IDLE_NONE;
    for (uint32_t now = 0; now < 2u * 3600u * 1000u; now += FRAME) {
        face_update(&f, now);
        seen[f.expr]++;
        if (f.idle != prev_idle && f.idle == IDLE_YAWN) yawns++;
        if (f.idle != prev_idle && f.idle == IDLE_GLANCE) glances++;
        prev_idle = f.idle;
    }
    for (int e = 0; e < EXPR_MOOD_COUNT; e++) {
        printf("  %-9s %5.1f%%\n", face_expression_name((expression_t)e),
               100.0 * seen[e] / (2.0 * 3600 * 1000 / FRAME));
        assert(seen[e] > 0);
    }
    printf("  yawns %d, glances %d in 2 h\n", yawns, glances);
    assert(seen[EXPR_LOWBAT] == 0);
    assert(yawns > 0 && glances > 0);
}

static void test_reactions(void) {
    static face_t f;
    uint32_t now = 5000;
    face_init(&f, 3, now);
    face_on_event(&f, INPUT_TAP, now);
    for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_SURPRISED);
    for (int i = 0; i < 60; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_HAPPY);  // TAP chains into happy

    face_on_event(&f, INPUT_DOUBLE_TAP, now);
    for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_LOVE);

    face_on_event(&f, INPUT_TILT_LEFT, now);
    for (int i = 0; i < 20; i++) face_update(&f, now += FRAME);
    assert(f.look_x < 0);
    for (int i = 0; i < 80; i++) face_update(&f, now += FRAME);
    assert(f.look_x == 0);  // came back
}

static void test_night(void) {
    static face_t f;
    static gfx_t g;
    uint32_t now = 0;
    face_init(&f, 9, now);
    face_set_night(&f, true, now);
    int awake_px = 0;
    for (; now < 3600u * 1000u; now += FRAME) {
        face_update(&f, now);
        assert(!f.blinking || now < 1000);
        if (now > 1000) assert(f.expr == EXPR_SLEEPY && f.idle == IDLE_NONE);
    }
    gfx_clear(&g);
    face_render(&f, &g);
    const int night_px = lit(&g);
    face_set_night(&f, false, now);
    for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_SURPRISED);
    show(&f, EXPR_NEUTRAL, &now);
    gfx_clear(&g);
    face_render(&f, &g);
    awake_px = lit(&g);
    printf("  lit pixels: night %d, neutral %d\n", night_px, awake_px);
    assert(night_px * 4 < awake_px);  // night really is darker
}

static void test_time_wrap(void) {
    static face_t f;
    uint32_t now = UINT32_MAX - 20000;
    face_init(&f, 11, now);
    int blinks = 0;
    bool was = false;
    for (int i = 0; i < 60000 / FRAME; i++) {
        face_update(&f, now += FRAME);
        if (f.blinking && !was) blinks++;
        was = f.blinking;
    }
    assert(blinks >= 60000 / (BLINK_MAX_MS + BLINK_DURATION_MS) - 1);
}

static void test_policy(void) {
    policy_t p;
    policy_init(&p);
    assert(!policy_update(&p, 0) && !p.night);
    const uint64_t night_at = NIGHT_CYCLE_AWAKE_MS + 1000;
    assert(policy_update(&p, night_at) && p.night);
    assert(policy_contrast(&p) == CONTRAST_NIGHT);
    // Input wakes it for the rest of this night...
    policy_on_event(&p, INPUT_TAP, night_at + 10);
    assert(policy_update(&p, night_at + 20) && !p.night);
    // ...but the next night comes as scheduled.
    const uint64_t cycle = (uint64_t)NIGHT_CYCLE_AWAKE_MS + NIGHT_CYCLE_NIGHT_MS;
    policy_update(&p, cycle + 5);
    assert(!p.night);
    assert(policy_update(&p, cycle + night_at) && p.night);
    // Long-press forces night during the day.
    policy_init(&p);
    policy_on_event(&p, INPUT_BTN_A_LONG, 100);
    assert(policy_update(&p, 200) && p.night);
    policy_on_event(&p, INPUT_BTN_A_SHORT, 300);
    assert(policy_update(&p, 400) && !p.night);

    // Burn-in: stays in range and actually moves.
    int moved = 0, px = 0, py = 0;
    for (int i = 0; i < 40; i++) {
        int dx, dy;
        policy_burnin_offset((uint64_t)i * BURNIN_SHIFT_MS, &dx, &dy);
        assert(dx >= -BURNIN_MAX_SHIFT_PX && dx <= BURNIN_MAX_SHIFT_PX);
        assert(dy >= -BURNIN_MAX_SHIFT_PX && dy <= BURNIN_MAX_SHIFT_PX);
        if (i && (dx != px || dy != py)) moved++;
        px = dx;
        py = dy;
    }
    assert(moved == 39);
}

static void test_battery(void) {
    assert(battery_percent_from_mv(3000) == 0);
    assert(battery_percent_from_mv(4300) == 100);
    int prev = -1;
    for (int mv = 3200; mv <= 4250; mv += 10) {  // monotonic
        const int p = battery_percent_from_mv(mv);
        assert(p >= prev && p >= 0 && p <= 100);
        prev = p;
    }
    assert(battery_level_from_mv(-1, BATTERY_OK) == BATTERY_UNKNOWN);
    assert(battery_level_from_mv(3900, BATTERY_OK) == BATTERY_OK);
    assert(battery_level_from_mv(BATTERY_MV_LOW, BATTERY_OK) == BATTERY_LOW);
    assert(battery_level_from_mv(BATTERY_MV_LOW + 20, BATTERY_LOW) == BATTERY_LOW);
    assert(battery_level_from_mv(BATTERY_MV_LOW + 60, BATTERY_LOW) == BATTERY_OK);
    assert(battery_level_from_mv(BATTERY_MV_CRITICAL, BATTERY_OK) == BATTERY_CRITICAL);

    static gfx_t g;
    gfx_clear(&g);
    const battery_status_t unknown = {BATTERY_UNKNOWN, -1, -1};
    hud_battery(&g, &unknown, 0);
    assert(lit(&g) == 0);  // icon hidden without an ADC
    const battery_status_t ok = {BATTERY_OK, 3900, 70};
    hud_battery(&g, &ok, 0);
    assert(lit(&g) > 0);
    assert(lit_in_margin(&g, BURNIN_MAX_SHIFT_PX) == 0);  // safe to shift
}

int main(void) {
#define RUN(t) (printf("%s\n", #t), t())
    RUN(test_gfx);
    RUN(test_every_expression_fits);
    RUN(test_blinks_at_random_intervals);
    RUN(test_mood_machine_visits_every_mood);
    RUN(test_reactions);
    RUN(test_night);
    RUN(test_time_wrap);
    RUN(test_policy);
    RUN(test_battery);
    printf("all tests passed\n");
    return 0;
}
