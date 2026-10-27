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
            // Dots and thin strokes light few pixels; just make sure it drew.
            if (e != EXPR_LOWBAT) assert(lit(&g) > 60);
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
    int yawns = 0, glances = 0, sneezes = 0, hums = 0, flies = 0, snows = 0, cues = 0;
    idle_kind_t prev_idle = IDLE_NONE;
    for (uint32_t now = 0; now < 2u * 3600u * 1000u; now += FRAME) {
        face_update(&f, now);
        seen[f.expr]++;
        if (f.idle != prev_idle && f.idle == IDLE_YAWN) yawns++;
        if (f.idle != prev_idle && f.idle == IDLE_GLANCE) glances++;
        if (f.idle != prev_idle && f.idle == IDLE_SNEEZE) sneezes++;
        if (f.idle != prev_idle && f.idle == IDLE_HUM) hums++;
        if (f.idle != prev_idle && f.idle == IDLE_FLY) flies++;
        if (f.idle != prev_idle && f.idle == IDLE_SNOW) snows++;
        cues += face_take_cue(&f) != CUE_NONE;
        prev_idle = f.idle;
    }
    for (int e = 0; e < EXPR_MOOD_COUNT; e++) {
        printf("  %-9s %5.1f%%\n", face_expression_name((expression_t)e),
               100.0 * seen[e] / (2.0 * 3600 * 1000 / FRAME));
        assert(seen[e] > 0);
    }
    printf("  yawns %d, glances %d, sneezes %d, hums %d, flies %d, snows %d in 2 h\n", yawns, glances,
           sneezes, hums, flies, snows);
    assert(seen[EXPR_LOWBAT] == 0 && seen[EXPR_SCARED] == 0);
    assert(yawns > 0 && glances > 0 && sneezes > 0 && hums > 0 && flies > 0 && snows > 0);
    assert(cues == sneezes + hums + flies + snows);  // each one asks for its sound, once
}

static void test_reactions(void) {
    static face_t f;
    uint32_t now = 5000;
    face_init(&f, 3, now);
    face_on_event(&f, INPUT_TAP, now);
    for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_SURPRISED);
    for (int i = 0; i < 60; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_HAPPY || f.expr == EXPR_LAUGH);  // TAP chains into a cheerful face

    face_on_event(&f, INPUT_DOUBLE_TAP, now);
    for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_LOVE);

    face_on_event(&f, INPUT_TILT_LEFT, now);
    for (int i = 0; i < 20; i++) face_update(&f, now += FRAME);
    assert(f.look_x < 0);
    for (int i = 0; i < 80; i++) face_update(&f, now += FRAME);
    assert(f.look_x == 0);  // came back

    // Shake: dizzy (the anger building up is test_rage).
    face_on_event(&f, INPUT_SHAKE, now);
    for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_DIZZY);
    assert(face_is_animating(&f));  // the spirals spin
    for (int i = 0; i < 300; i++) face_update(&f, now += FRAME);  // calm down

    // Petted on the touch pad: hearts (deeper stages are test_petting),
    // happy once let go after a short cuddle.
    face_on_event(&f, INPUT_TOUCH_HOLD, now);
    for (int i = 0; i < 60; i++) face_update(&f, now += FRAME);  // 2 s
    assert(f.expr == EXPR_BLISS);
    face_on_event(&f, INPUT_TOUCH, now);  // a pat meanwhile doesn't break the cuddle
    for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_BLISS);
    face_on_event(&f, INPUT_TOUCH_END, now);
    for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_HAPPY);
    face_on_event(&f, INPUT_TICKLE, now);
    for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_LAUGH);

    // The mic: a shout scares it (with a sound), blowing makes it sneeze,
    // talking makes it look up and listen until the talk ends.
    face_on_event(&f, INPUT_LOUD, now);
    for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_SCARED && face_take_cue(&f) == CUE_SCARED);
    face_on_event(&f, INPUT_BLOW, now);
    assert(f.idle == IDLE_BLOWN && face_take_cue(&f) == CUE_WIND);
    for (int i = 0; i < 20; i++) face_update(&f, now += FRAME);
    assert(f.look_x != 0);  // pushed aside by the wind
    for (int i = 0; i < 25; i++) face_update(&f, now += FRAME);
    assert(f.idle == IDLE_SNEEZE && face_take_cue(&f) == CUE_SNEEZE);
    for (int i = 0; i < 100; i++) face_update(&f, now += FRAME);
    assert(f.idle == IDLE_NONE);
    face_on_event(&f, INPUT_HEAR, now);
    for (int i = 0; i < 60; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_SURPRISED && f.look_y < 0);
    face_on_event(&f, INPUT_TALK, now);
    for (int i = 0; i < 30; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_HAPPY && f.look_y == 0);

    // A pets it: something cheerful, never the face already showing.
    for (int k = 0; k < 20; k++) {
        const expression_t before = f.has_pending ? f.pending : f.expr;
        face_on_event(&f, INPUT_BTN_A_SHORT, now);
        assert(f.pending != before);
        assert(f.pending == EXPR_HAPPY || f.pending == EXPR_LAUGH || f.pending == EXPR_WINK ||
               f.pending == EXPR_EXCITED);
        for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    }

    // B is the menu's (main.c): the face ignores it.
    show(&f, EXPR_NEUTRAL, &now);
    face_on_event(&f, INPUT_BTN_B_SHORT, now);
    assert(!f.has_pending && f.expr == EXPR_NEUTRAL);
    // B held steps through every face except the low-battery one, then wraps.
    int seen[EXPR_COUNT] = {0};
    for (int k = 0; k < EXPR_LOWBAT; k++) {
        face_on_event(&f, INPUT_BTN_B_LONG, now);
        for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
        seen[f.expr]++;
    }
    for (int e = 0; e < EXPR_LOWBAT; e++) assert(seen[e] == 1);
    assert(f.expr == EXPR_NEUTRAL);

    // Animations end, so the main loop can drop to the slow frame rate.
    show(&f, EXPR_HAPPY, &now);
    f.expr_since = now;
    assert(face_is_animating(&f));
    for (int i = 0; i < 60; i++) face_update(&f, now += FRAME);
    f.blinking = false;
    f.next_blink = now + 100000;
    assert(!face_is_animating(&f));
}

static void run_ms(face_t *f, uint32_t ms, uint32_t *now) {
    for (uint32_t t = 0; t < ms; t += FRAME) face_update(f, *now += FRAME);
}

// Rendered, it draws something and stays inside the burn-in margin.
static void assert_fits(const face_t *f) {
    static gfx_t g;
    gfx_clear(&g);
    face_render(f, &g);
    assert(lit(&g) > 60 && lit_in_margin(&g, BURNIN_MAX_SHIFT_PX) == 0);
}

static void test_rage(void) {
    static face_t f;
    static gfx_t g;
    uint32_t now = 5000;
    face_init(&f, 41, now);

    // Shaken on and on (the sensor reports a SHAKE every ~1.5 s): dizzy,
    // grumbling, furious twice, then knocked out.
    static const face_cue_t CUE[5] = {CUE_NONE, CUE_GRUMBLE, CUE_FURY, CUE_FURY, CUE_KO};
    static const expression_t EXPR[5] = {EXPR_DIZZY, EXPR_DIZZY, EXPR_FURIOUS, EXPR_FURIOUS, EXPR_KO};
    for (int k = 0; k < 5; k++) {
        face_on_event(&f, INPUT_SHAKE, now);
        assert(face_take_cue(&f) == CUE[k]);
        run_ms(&f, 300, &now);
        assert(f.expr == EXPR[k]);
        if (EXPR[k] == EXPR_FURIOUS) {
            assert(face_is_animating(&f));
            f.now = f.expr_since + 100;  // boiling over: the screen flashes
            gfx_clear(&g);
            face_render(&f, &g);
            assert(lit(&g) > GFX_W * GFX_H / 2);
            f.now = f.expr_since + 600;  // then steam, inside the margin
            assert_fits(&f);
        }
        run_ms(&f, 1200, &now);
    }

    // Comes to, sulking: cross and looking away. Two pats: "hmph", and it
    // turns the other way. The third wins it over.
    run_ms(&f, 3000, &now);
    assert(f.expr == EXPR_ANGRY && f.look_x != 0);
    const int away = f.look_tx;
    face_on_event(&f, INPUT_TOUCH, now);
    assert(face_take_cue(&f) == CUE_HMPH);
    run_ms(&f, 500, &now);
    assert(f.expr == EXPR_ANGRY && f.look_tx == -away);
    face_on_event(&f, INPUT_BTN_A_SHORT, now);
    assert(face_take_cue(&f) == CUE_HMPH);
    run_ms(&f, 500, &now);
    assert(f.expr == EXPR_ANGRY);
    face_on_event(&f, INPUT_TOUCH, now);
    assert(face_take_cue(&f) == CUE_FORGIVEN);
    run_ms(&f, 500, &now);
    assert(f.expr == EXPR_LOVE && f.look_tx == 0);

    // Shakes far apart never build up.
    run_ms(&f, 10000, &now);
    for (int k = 0; k < 6; k++) {
        face_on_event(&f, INPUT_SHAKE, now);
        assert(face_take_cue(&f) == CUE_NONE);
        run_ms(&f, 6000, &now);
    }

    // Knocked out again: a cuddle forgives at once; left alone, the sulk
    // wears off by itself.
    for (int k = 0; k < 5; k++) {
        face_on_event(&f, INPUT_SHAKE, now);
        run_ms(&f, 1500, &now);
    }
    assert(face_take_cue(&f) == CUE_KO);
    run_ms(&f, 3000, &now);
    assert(f.expr == EXPR_ANGRY);
    face_on_event(&f, INPUT_TOUCH_HOLD, now);
    assert(face_take_cue(&f) == CUE_FORGIVEN && f.petting);
    run_ms(&f, 500, &now);
    assert(f.expr == EXPR_BLISS);
    face_on_event(&f, INPUT_TOUCH_END, now);
    run_ms(&f, 10000, &now);
    for (int k = 0; k < 5; k++) {
        face_on_event(&f, INPUT_SHAKE, now);
        run_ms(&f, 1500, &now);
    }
    run_ms(&f, 25000, &now);
    assert(f.sulk_dir == 0);  // over (the eyes may be glancing about again)
    face_take_cue(&f);  // the KO's, and any idle sneeze since
    face_on_event(&f, INPUT_TOUCH, now);  // not sulking: a pat is just a pat
    assert(face_take_cue(&f) == CUE_NONE);
    run_ms(&f, 500, &now);
    assert(f.expr == EXPR_HAPPY);
}

static void test_petting(void) {
    static face_t f;
    static gfx_t g;
    uint32_t now = 5000;
    face_init(&f, 43, now);

    // One hold: squished, nuzzling, melting, heart overload, each with its
    // cue, all inside the margin.
    face_on_event(&f, INPUT_TOUCH_HOLD, now);
    run_ms(&f, 1000, &now);
    assert(f.expr == EXPR_BLISS && f.pet_stage == 0 && face_take_cue(&f) == CUE_NONE);
    assert(face_is_animating(&f) && face_heartbeat_ms(&f) == 0);
    assert_fits(&f);
    run_ms(&f, 700, &now);
    assert(f.pet_stage == 1 && face_take_cue(&f) == CUE_BLISS);
    assert_fits(&f);
    run_ms(&f, 1800, &now);
    assert(f.pet_stage == 2 && face_take_cue(&f) == CUE_MELT);
    run_ms(&f, 1200, &now);
    assert_fits(&f);  // dripping and puddling
    run_ms(&f, 400, &now);
    assert(f.pet_stage == 3 && face_take_cue(&f) == CUE_OVERLOAD && f.expr == EXPR_BLISS);
    assert_fits(&f);
    // The heart beats faster and faster.
    const uint32_t b0 = face_heartbeat_ms(&f);
    run_ms(&f, 1500, &now);
    assert(b0 >= 650 && face_heartbeat_ms(&f) < b0 - 100);
    // Let go here (the TTP223 gives up by itself about now): the finale.
    face_on_event(&f, INPUT_TOUCH_END, now);
    assert(face_take_cue(&f) == CUE_HEARTS && f.fx == FX_HEARTS && !f.petting);
    assert(face_heartbeat_ms(&f) == 0);
    run_ms(&f, 600, &now);
    gfx_clear(&g);
    face_render(&f, &g);
    assert(lit(&g) > 200);  // big hearts and the burst
    run_ms(&f, 1300, &now);
    assert(f.fx == FX_NONE && f.expr == EXPR_LOVE);
    run_ms(&f, 1500, &now);
    assert(f.expr == EXPR_HAPPY);

    // Let go while melting: it snaps back into shape. Early: a happy hop.
    face_on_event(&f, INPUT_TOUCH_HOLD, now);
    run_ms(&f, 4000, &now);
    face_on_event(&f, INPUT_TOUCH_END, now);
    run_ms(&f, 300, &now);
    assert(f.expr == EXPR_SURPRISED && face_take_cue(&f) == CUE_MELT);
    run_ms(&f, 800, &now);
    assert(f.expr == EXPR_HAPPY);
    face_on_event(&f, INPUT_TOUCH_HOLD, now);
    run_ms(&f, 1000, &now);
    face_on_event(&f, INPUT_TOUCH_END, now);
    run_ms(&f, 300, &now);
    assert(f.expr == EXPR_HAPPY && f.fx == FX_NONE);
    run_ms(&f, 5000, &now);

    // A pad that holds on: the finale anyway, then content hearts until it
    // lets go; a pad stuck on for good ends the cuddle by itself.
    face_take_cue(&f);
    face_on_event(&f, INPUT_TOUCH_HOLD, now);
    run_ms(&f, 7600, &now);
    assert(f.pet_stage == 4 && f.fx == FX_HEARTS && face_take_cue(&f) == CUE_HEARTS);
    run_ms(&f, 3000, &now);
    assert(f.petting && f.expr == EXPR_LOVE && f.fx == FX_NONE);
    run_ms(&f, 51000, &now);
    assert(!f.petting);
}

static void test_hang(void) {
    static face_t f;
    static gfx_t g;
    uint32_t now = 5000;
    face_init(&f, 47, now);
    face_on_event(&f, INPUT_TOUCH_HOLD, now);
    run_ms(&f, 1000, &now);

    // Hangs: frozen, the cuddle is over, and only shaking does anything.
    face_hang(&f, now);
    assert(face_hung(&f) && !f.petting);
    run_ms(&f, 1000, &now);
    assert_fits(&f);
    face_on_event(&f, INPUT_BTN_A_SHORT, now);
    assert(face_take_cue(&f) == CUE_ERROR);
    face_on_event(&f, INPUT_TOUCH, now);
    assert(face_take_cue(&f) == CUE_ERROR);
    face_on_event(&f, INPUT_TILT_LEFT, now);
    face_on_event(&f, INPUT_TALK, now);
    assert(face_take_cue(&f) == CUE_NONE && f.look_tx == 0);
    run_ms(&f, 6000, &now);  // "SHAKE ME!" by now
    assert(face_hung(&f) && f.fx == FX_HANG);
    assert_fits(&f);

    // Three shakes: two jolts, then it reboots; no anger builds up meanwhile.
    for (int k = 0; k < FACE_HANG_FIXES - 1; k++) {
        face_on_event(&f, INPUT_SHAKE, now);
        assert(face_take_cue(&f) == CUE_JOLT);
        gfx_clear(&g);
        face_render(&f, &g);
        assert(lit(&g) > 60);
        run_ms(&f, 1500, &now);
    }
    face_on_event(&f, INPUT_SHAKE, now);
    assert(face_take_cue(&f) == CUE_REBOOT && f.fx == FX_REBOOT && face_hung(&f));
    assert(f.rage == 0);
    face_on_event(&f, INPUT_SHAKE, now);  // busy rebooting: ignored
    assert(face_take_cue(&f) == CUE_NONE);
    run_ms(&f, 1000, &now);
    assert_fits(&f);  // the boot screen
    run_ms(&f, FACE_REBOOT_MS, &now);
    assert(!face_hung(&f) && face_take_cue(&f) == CUE_BOOTED);
    run_ms(&f, 300, &now);
    assert(f.expr == EXPR_EXCITED);

    // Left hung, it reboots by itself.
    face_hang(&f, now);
    run_ms(&f, FACE_HANG_MAX_MS + 100, &now);
    assert(f.fx == FX_REBOOT && face_take_cue(&f) == CUE_REBOOT);
    run_ms(&f, FACE_REBOOT_MS + 100, &now);
    assert(!face_hung(&f));
}

// Renders differ from the plain face (something extra is drawn).
static bool differs(const face_t *f, const gfx_t *plain) {
    static gfx_t g;
    gfx_clear(&g);
    face_render(f, &g);
    return memcmp(g.buf, plain->buf, sizeof(g.buf)) != 0;
}

static void test_animations(void) {
    static face_t f;
    static gfx_t g, plain;
    uint32_t now = 5000;
    face_init(&f, 53, now);

    // Power-on: a line, then eyes springing open, then a look around.
    face_boot(&f, now);
    run_ms(&f, 100, &now);
    gfx_clear(&g);
    face_render(&f, &g);
    assert(lit(&g) > 20 && lit(&g) < 400);  // just the line
    run_ms(&f, 700, &now);
    assert_fits(&f);
    run_ms(&f, 1000, &now);
    assert(f.fx == FX_NONE);
    run_ms(&f, 300, &now);
    assert(f.expr == EXPR_HAPPY);

    show(&f, EXPR_NEUTRAL, &now);
    gfx_clear(&plain);
    face_render(&f, &plain);

    // A fly: the eyes follow it about, it lands, gets blinked off, leaves.
    face_start_idle(&f, IDLE_FLY, now);
    assert(face_take_cue(&f) == CUE_BUZZ);
    int min_x = 99, max_x = -99;
    bool blinked = false;
    for (uint32_t t = 0; t < 5400; t += FRAME) {
        face_update(&f, now += FRAME);
        if (f.look_x < min_x) min_x = f.look_x;
        if (f.look_x > max_x) max_x = f.look_x;
        blinked |= f.blinking && t > 4400;
        if (t > 3800 && t < 4400) assert(f.look_y < 0);  // looking up at it on the eye
    }
    assert(min_x < -5 && max_x > 5 && blinked);
    run_ms(&f, 400, &now);
    assert(f.idle == IDLE_NONE);

    // Snow: happy, looking up, flakes falling; maybe a sneeze after.
    show(&f, EXPR_NEUTRAL, &now);
    face_start_idle(&f, IDLE_SNOW, now);
    assert(face_take_cue(&f) == CUE_SNOW);
    run_ms(&f, 2000, &now);
    assert(f.expr == EXPR_HAPPY && f.look_y < 0 && face_is_animating(&f));
    assert(differs(&f, &plain));
    run_ms(&f, 4100, &now);
    assert(f.idle == IDLE_NONE || f.idle == IDLE_SNEEZE);
    run_ms(&f, 2000, &now);

    // Tickled, then tickled again: rolling with laughter, off and back on.
    face_on_event(&f, INPUT_TICKLE, now);
    run_ms(&f, 1000, &now);
    assert(f.expr == EXPR_LAUGH && f.fx == FX_NONE);
    face_on_event(&f, INPUT_TICKLE, now);
    assert(face_take_cue(&f) == CUE_ROFL && f.fx == FX_ROLL);
    run_ms(&f, 900, &now);
    gfx_clear(&g);
    face_render(&f, &g);
    assert(lit(&g) > 60);
    run_ms(&f, 1000, &now);
    assert(f.fx == FX_NONE);
    // A tickle long after is just a laugh again.
    run_ms(&f, 5000, &now);
    face_on_event(&f, INPUT_TICKLE, now);
    assert(f.fx == FX_NONE && face_take_cue(&f) == CUE_NONE);
    run_ms(&f, 4000, &now);

    // Tapping on and on (the pad gives TOUCH, TOUCH, TICKLE, ...): one
    // laugh that keeps going, a single roll, never flickering back to happy.
    show(&f, EXPR_NEUTRAL, &now);
    run_ms(&f, 9000, &now);  // the last roll is long past
    int rolls = 0;
    for (int k = 0; k < 30; k++) {
        const bool was_rolling = f.fx == FX_ROLL;
        face_on_event(&f, k % 3 == 2 ? INPUT_TICKLE : INPUT_TOUCH, now);
        rolls += f.fx == FX_ROLL && !was_rolling;
        for (uint32_t t = 0; t < 300; t += FRAME) {
            face_update(&f, now += FRAME);
            if (k >= 3) {
                assert(f.expr == EXPR_LAUGH && (!f.has_pending || f.pending == EXPR_LAUGH));
                assert(face_laughing(&f) && face_is_animating(&f));
            }
        }
    }
    assert(rolls == 1);
    run_ms(&f, 4000, &now);
    assert(!face_laughing(&f));

    // Talking back: a mouth that moves, then stops.
    show(&f, EXPR_HAPPY, &now);
    face_talk(&f, 4, now);
    run_ms(&f, 60, &now);
    assert(face_is_animating(&f));
    bool open_seen = false, shut_seen = false;
    for (int i = 0; i < 18; i++) {
        gfx_clear(&g);
        face_render(&f, &g);
        int mouth = 0;
        for (int y = 50; y < 62; y++)
            for (int x = 56; x < 72; x++) mouth += gfx_get_pixel(&g, x, y);
        open_seen |= mouth > 30;
        shut_seen |= mouth > 0 && mouth <= 20;
        face_update(&f, now += FRAME);
    }
    assert(open_seen && shut_seen);
    run_ms(&f, 500, &now);
    f.blinking = false;
    f.next_blink = now + 100000;
    f.expr_since = now - 60000;
    assert(!face_is_animating(&f));

    // Waking with a stretch stays inside the margin all the way.
    face_set_night(&f, true, now);
    run_ms(&f, 1000, &now);
    face_set_night(&f, false, now);
    for (int i = 0; i < 2200 / FRAME; i++) {
        face_update(&f, now += FRAME);
        if (f.idle == IDLE_STRETCH) assert_fits(&f);
    }
}

// Feeds the face the same sample for `ms`, a frame at a time.
static void feel(face_t *f, int x, int y, int z, uint32_t ms, uint32_t *now) {
    const int mg[3] = {x, y, z};
    for (uint32_t t = 0; t < ms; t += FRAME) {
        face_feel(f, mg, true, *now);
        face_update(f, *now += FRAME);
    }
}

static void test_body_sense(void) {
    static face_t f;
    static gfx_t g;
    uint32_t now = 5000;
    face_init(&f, 21, now);
    show(&f, EXPR_NEUTRAL, &now);

    // Upright and still: nothing happens.
    feel(&f, 0, 1000, 0, 2000, &now);
    assert(f.look_tx == 0 && !f.upside && face_take_cue(&f) == CUE_NONE);

    // Tilted right (screen frame reads -x): the eyes roll right; level: back.
    feel(&f, -400, 900, 0, 600, &now);
    assert(f.look_x > 6);
    feel(&f, 0, 1000, 0, 1500, &now);
    assert(f.look_x == 0);

    // A quick flip is not a panic; held upside down it is, for as long as
    // it stays that way; righted, relief.
    feel(&f, 0, -1000, 0, 300, &now);
    feel(&f, 0, 1000, 0, 300, &now);
    assert(!f.upside && face_take_cue(&f) == CUE_NONE);
    feel(&f, 0, -1000, 0, 800, &now);
    assert(f.upside && face_take_cue(&f) == CUE_SCARED);
    feel(&f, 0, -1000, 0, 10000, &now);
    assert(f.expr == EXPR_SCARED && face_is_animating(&f));
    gfx_clear(&g);
    face_render(&f, &g);
    assert(lit(&g) > 60 && lit_in_margin(&g, BURNIN_MAX_SHIFT_PX) == 0);
    feel(&f, 0, 1000, 0, 800, &now);
    assert(!f.upside && face_take_cue(&f) == CUE_RELIEF);
    feel(&f, 0, 1000, 0, 400, &now);
    assert(f.expr == EXPR_HAPPY);
    // Uncalibrated, "up" is unknown: no panic.
    for (int i = 0; i < 60; i++) {
        const int mg[3] = {0, -1000, 0};
        face_feel(&f, mg, false, now);
        face_update(&f, now += FRAME);
    }
    assert(!f.upside);

    // Dropped: weightless for two samples, WHEE; not again straight away.
    feel(&f, 0, 1000, 0, 4000, &now);
    feel(&f, 20, 50, 30, 100, &now);
    assert(face_take_cue(&f) == CUE_WHEE);
    feel(&f, 0, 1000, 0, 200, &now);
    feel(&f, 20, 50, 30, 100, &now);
    assert(face_take_cue(&f) == CUE_NONE);

    // Lying still for minutes, then picked up: hello.
    feel(&f, 0, 0, 1000, 100000, &now);
    (void)face_take_cue(&f);
    feel(&f, 300, 200, 900, 70, &now);
    assert(face_take_cue(&f) == CUE_HI);
    // Moved again soon after: no second hello.
    feel(&f, 0, 0, 1000, 2000, &now);
    feel(&f, 300, 200, 900, 70, &now);
    assert(face_take_cue(&f) == CUE_NONE);

    // Asleep it feels nothing.
    face_set_night(&f, true, now);
    feel(&f, 0, -1000, 0, 3000, &now);
    assert(!f.upside && face_take_cue(&f) == CUE_NONE);
}

static void test_needs(void) {
    static face_t f;
    static gfx_t g;
    uint32_t now = 0;
    face_init(&f, 23, now);
    // Hungry: much sadder over an hour than a cared-for pet.
    int sad_ok = 0, sad_hungry = 0;
    for (int pass = 0; pass < 2; pass++) {
        face_init(&f, 23, 0);
        face_set_needs(&f, pass ? PET_NEED_FOOD : 0);
        for (now = 0; now < 3600u * 1000u; now += FRAME) {
            face_update(&f, now);
            *(pass ? &sad_hungry : &sad_ok) += f.expr == EXPR_SAD;
        }
    }
    printf("  sad frames: cared for %d, hungry %d\n", sad_ok, sad_hungry);
    assert(sad_hungry > 3 * sad_ok);

    // The thought bubble shows now and then, inside the burn-in margin, with
    // the eyes glanced fully right.
    face_set_needs(&f, PET_NEED_FOOD | PET_NEED_FUN);
    show(&f, EXPR_NEUTRAL, &now);
    f.reacting = false;
    int shown = 0;
    for (int s = 0; s < 30; s++) {
        now += 1000;
        f.now = now;
        f.look_x = f.look_tx = 12;
        gfx_clear(&g);
        face_render(&f, &g);
        assert(lit_in_margin(&g, BURNIN_MAX_SHIFT_PX) == 0);
        shown += gfx_get_pixel(&g, 117, 3);  // the bubble's top edge
    }
    assert(shown >= 6 && shown <= 10);  // 4 s of every 15
}

static void test_fx(void) {
    static face_t f;
    static gfx_t g, plain;
    uint32_t now = 5000;
    face_init(&f, 31, now);
    show(&f, EXPR_NEUTRAL, &now);
    gfx_clear(&plain);
    face_render(&f, &plain);

    // Alarm: sad underneath, the screen flashes, then it ends on its own.
    face_fx(&f, FX_ALARM, NULL, now, 4000);
    for (int i = 0; i < 10; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_SAD && face_is_animating(&f));
    f.now = f.fx_since + 160;  // an inverted flash frame
    gfx_clear(&g);
    face_render(&f, &g);
    assert(lit(&g) > GFX_W * GFX_H / 2);
    for (int i = 0; i < 150; i++) face_update(&f, now += FRAME);
    assert(f.fx == FX_NONE);

    // Glitch: the picture is torn and shows the log line, then "what?".
    face_fx(&f, FX_GLITCH, "SIGNAL LOST", now, 1800);
    for (int i = 0; i < 20; i++) face_update(&f, now += FRAME);
    gfx_clear(&g);
    face_render(&f, &g);
    assert(memcmp(g.buf, plain.buf, sizeof(g.buf)) != 0);
    for (int i = 0; i < 60; i++) face_update(&f, now += FRAME);
    assert(f.fx == FX_NONE);
    for (int i = 0; i < 20; i++) face_update(&f, now += FRAME);
    assert(f.expr == EXPR_SURPRISED);

    // Singing lasts until stopped, and the mood machine stays out of it.
    face_fx(&f, FX_SING, NULL, now, 0);
    for (int i = 0; i < 2000; i++) face_update(&f, now += FRAME);
    assert(f.fx == FX_SING && face_is_animating(&f));
    gfx_clear(&g);
    face_render(&f, &g);
    assert(lit(&g) > 60 && lit_in_margin(&g, BURNIN_MAX_SHIFT_PX) == 0);
    face_fx_stop(&f);
    assert(f.fx == FX_NONE);
    for (int i = 0; i < 20; i++) face_update(&f, now += FRAME);
    assert(!f.has_pending || f.pending < EXPR_MOOD_COUNT);  // back to moods
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
    face_set_night(&f, false, now);  // woken: a big stretch
    assert(f.idle == IDLE_STRETCH && face_take_cue(&f) == CUE_STRETCH);
    for (int i = 0; i < 30; i++) face_update(&f, now += FRAME);
    assert(face_is_animating(&f));
    for (int i = 0; i < 60; i++) face_update(&f, now += FRAME);
    assert(f.idle == IDLE_NONE);
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
    // A long press is just input now; the menu's SLEEP forces night until
    // the next input.
    policy_init(&p);
    policy_on_event(&p, INPUT_BTN_A_LONG, 100);
    assert(!policy_update(&p, 200) && !p.night);
    policy_sleep(&p);
    assert(policy_update(&p, 300) && p.night);
    policy_on_event(&p, INPUT_BTN_A_SHORT, 400);
    assert(policy_update(&p, 500) && !p.night);

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
    RUN(test_rage);
    RUN(test_petting);
    RUN(test_hang);
    RUN(test_animations);
    RUN(test_body_sense);
    RUN(test_needs);
    RUN(test_fx);
    RUN(test_night);
    RUN(test_time_wrap);
    RUN(test_policy);
    RUN(test_battery);
    printf("all tests passed\n");
    return 0;
}
