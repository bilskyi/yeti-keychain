// The 67 easter egg: six quick presses of A on the face, then a shake
// (app_egg_watch). A show of its own, about seven seconds, then back to an
// excited face. Any button but C is ignored meanwhile; C ends it early.
//
//   0.0  the screen charges up: glitching bands, a rising whine
//   0.7  a huge 6 slams in from the left: flash, the screen shakes
//   1.1  the 7 slams in from the right
//   1.5  shockwaves ring out, the 67 throbs, sparkles
//   2.7  the 6-7 hands: palms up, swinging in turn, SIX SEVEN scrolling past
//   5.0  the 67 spins
//   5.6  and blows up into flying blocks
//   6.3  a small 67 is left, blinking, to a fanfare
//
// Everything is drawn from the time alone (no state but the clock), so a
// frame can be drawn for any moment.

#include "app/app_internal.h"

#define T_SIX 700
#define T_SIX_HIT 1100
#define T_SEVEN_HIT 1500
#define T_MEME 2700
#define T_SPIN 5000
#define T_BOOM 5600
#define T_FINALE 6300
#define BEAT_MS 260

#define BIG 80  // the digits' scale, 1/16: 5 px a font pixel

static uint32_t hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static const struct {
    uint32_t at;
    app_sound_t snd;
} CUES[] = {
    {0, SND_EGG_RISE}, {T_SIX_HIT, SND_EGG_SIX}, {T_SEVEN_HIT, SND_EGG_SEVEN},
    {T_BOOM, SND_EGG_BOOM}, {T_FINALE, SND_RECORD},
};

void egg_start(app_t *a, uint32_t now) {
    a->g.egg.t0 = now;
    a->g.egg.cued = 0;
}

bool egg_event(app_t *a, input_event_t ev, uint32_t now) {
    (void)a;
    (void)now;
    return ev != INPUT_BTN_C_SHORT;  // C: back to the face; the rest is the show
}

void egg_update(app_t *a, uint32_t now) {
    egg_t *e = &a->g.egg;
    const uint32_t t = now - e->t0;
    for (unsigned i = 0; i < sizeof(CUES) / sizeof(CUES[0]); i++) {
        if (!(e->cued & (1u << i)) && t >= CUES[i].at) {
            e->cued |= 1u << i;
            app_play(a, CUES[i].snd);
        }
    }
    // The hands: a tick on each swing (bits 8 and up count the beats).
    if (t >= T_MEME && t < T_SPIN) {
        const uint32_t beat = (t - T_MEME) / BEAT_MS + 1;
        if (beat != e->cued >> 8) {
            e->cued = (e->cued & 0xFF) | beat << 8;
            app_play(a, SND_EGG_TICK);
        }
    }
    if (t >= EGG_MS) {
        a->mood = EXPR_EXCITED;
        app_close(a);
    }
}

// One digit, each font pixel a block: sx, sy are the scale in 1/16 px.
static void big_char(gfx_t *g, char c, int x, int y, int sx, int sy) {
    const uint8_t *cols = font_glyph(c);
    for (int col = 0; col < FONT_W; col++) {
        const int x0 = x + col * sx / 16, x1 = x + (col + 1) * sx / 16;
        for (int row = 0; row < FONT_H; row++) {
            if (!(cols[col] >> row & 1)) continue;
            const int y0 = y + row * sy / 16, y1 = y + (row + 1) * sy / 16;
            gfx_fill_rect(g, x0, y0, x1 - x0 > 0 ? x1 - x0 : 1, y1 - y0, GFX_WHITE);
        }
    }
}

// "67" centred on cx, top at y; lift6 / lift7 raise one digit.
static void big_67(gfx_t *g, int cx, int y, int sx, int sy, int lift6, int lift7) {
    const int w = FONT_W * sx / 16, gap = sx / 16;
    big_char(g, '6', cx - gap / 2 - w, y - lift6, sx, sy);
    big_char(g, '7', cx + (gap + 1) / 2, y - lift7, sx, sy);
}

static void invert(gfx_t *g) {
    for (int i = 0; i < GFX_FB_BYTES; i++) g->buf[i] = (uint8_t)~g->buf[i];
}

static void sparkle(gfx_t *g, int x, int y, int r) {
    gfx_hline(g, x - r, x + r, y, GFX_WHITE);
    gfx_line(g, x, y - r, x, y + r, GFX_WHITE);
}

// A hand, palm up, on its forearm.
static void hand(gfx_t *g, int cx, int cy) {
    gfx_fill_rect(g, cx - 2, cy + 2, 5, GFX_H - cy, GFX_WHITE);
    gfx_fill_ellipse(g, cx, cy, 9, 3, GFX_WHITE);
    gfx_fill_rect(g, cx - 9, cy - 4, 2, 4, GFX_WHITE);  // fingertips curling up
    gfx_fill_rect(g, cx + 8, cy - 4, 2, 4, GFX_WHITE);
    gfx_fill_rect(g, cx - 5, cy - 1, 11, 1, GFX_BLACK);  // the hollow of the palm
}

static void draw_charge(gfx_t *g, uint32_t t) {
    // Bands of static, more and more of them, and the 6 rushing in after.
    const int bands = 2 + (int)t / 60;
    for (int i = 0; i < bands; i++) {
        const uint32_t h = hash((t / 40) * 131 + (uint32_t)i);
        const int y = (int)(h % GFX_H), x = (int)(h >> 8) % GFX_W, w = 8 + (int)(h >> 16) % 40;
        gfx_hline(g, x, x + w, y, GFX_WHITE);
        if (h & 0x10000000u) gfx_hline(g, x + 3, x + w / 2, y + 1, GFX_WHITE);
    }
}

static void draw_slam(gfx_t *g, uint32_t t) {
    const int w = FONT_W * BIG / 16, cx = 64, y = 14;
    const int six_x = cx - 2 - w, seven_x = cx + 3;
    if (t < T_SIX_HIT) {  // the 6 flies in, trailing speed lines
        const int k = (int)(t - T_SIX) * 256 / (T_SIX_HIT - T_SIX);
        const int x = -30 + (six_x + 30) * (65536 - (256 - k) * (256 - k)) / 65536;
        for (int r = 0; r < 5; r++) gfx_hline(g, x - 30 + r * 4, x - 2, y + 4 + r * 7, GFX_WHITE);
        big_char(g, '6', x, y, BIG, BIG);
        return;
    }
    big_char(g, '6', six_x, y, BIG, BIG);
    if (t < T_SEVEN_HIT) {
        const int k = (int)(t - T_SIX_HIT) * 256 / (T_SEVEN_HIT - T_SIX_HIT);
        const int x = GFX_W + 4 - (GFX_W + 4 - seven_x) * (65536 - (256 - k) * (256 - k)) / 65536;
        for (int r = 0; r < 5; r++) gfx_hline(g, x + w + 2, x + w + 30 - r * 4, y + 4 + r * 7, GFX_WHITE);
        big_char(g, '7', x, y, BIG, BIG);
        return;
    }
    big_char(g, '7', seven_x, y, BIG, BIG);
}

static void draw_rings(gfx_t *g, uint32_t t) {
    const int dt = (int)(t - T_SEVEN_HIT);
    for (int i = 0; i < 3; i++) {
        if (dt < i * 220) continue;
        const int r = 30 + (dt - i * 220) * 90 / 1000;  // from just outside the digits
        if (r > 80) continue;
        const int n = r * 6;
        for (int k = 0; k < n; k += r > 40 ? 3 : 1) {
            const int a = k * 64 / n;
            gfx_pixel(g, 64 + app_sin64(a + 16) * r / 256, 31 + app_sin64(a) * r / 256, GFX_WHITE);
        }
    }
    const bool throb = dt % 300 < 90;
    const int s = throb ? BIG + 12 : BIG;
    big_67(g, 64, throb ? 12 : 14, s, s, 0, 0);
    for (int i = 0; i < 8; i++) {
        const uint32_t h = hash(t / 120 * 17 + (uint32_t)i);
        const int x = (int)(h % 120) + 4, y = (int)(h >> 8) % 56 + 4;
        if (x > 30 && x < 98 && y > 8 && y < 54) continue;  // keep off the digits
        sparkle(g, x, y, 1 + (int)(h >> 20) % 2);
    }
}

static void draw_meme(gfx_t *g, uint32_t t) {
    const int beat = (int)(t - T_MEME) / BEAT_MS;
    const bool left_up = beat % 2 == 0;
    hand(g, 18, left_up ? 26 : 42);
    hand(g, 110, left_up ? 42 : 26);
    big_67(g, 64, 14, 64, 64, left_up ? 5 : -3, left_up ? -3 : 5);
    // SIX SEVEN, scrolling along the bottom.
    static const char LINE[] = "SIX SEVEN  SIX SEVEN  SIX SEVEN  ";
    const int len = (int)sizeof(LINE) - 1, half = len / 3 * FONT_ADVANCE * 1;
    const int x = -(int)((t - T_MEME) / 18 % (uint32_t)half);
    gfx_fill_rect(g, 0, 55, GFX_W, 9, GFX_BLACK);
    gfx_hline(g, 0, GFX_W - 1, 55, GFX_WHITE);
    font_draw(g, x, 57, LINE, 1, GFX_WHITE);
    if (beat % 4 == 3 && (int)(t - T_MEME) % BEAT_MS < 60) invert(g);  // a flash every bar
}

static void draw_spin(gfx_t *g, uint32_t t) {
    const int a = (int)(t - T_SPIN) * 128 / (T_BOOM - T_SPIN);  // two turns
    int c = app_sin64(a + 16);
    c = c < 0 ? -c : c;
    big_67(g, 64, 14, BIG * c / 256 > 2 ? BIG * c / 256 : 2, BIG, 0, 0);
}

// The digits' blocks flying apart: each from where it was, outward and up,
// then falling.
static void draw_boom(gfx_t *g, uint32_t t) {
    const int tau = (int)(t - T_BOOM);  // ms
    const int w = FONT_W * BIG / 16;
    int idx = 0;
    for (int d = 0; d < 2; d++) {
        const uint8_t *cols = font_glyph(d ? '7' : '6');
        const int ox = d ? 64 + 3 : 64 - 2 - w;
        for (int col = 0; col < FONT_W; col++) {
            for (int row = 0; row < FONT_H; row++) {
                if (!(cols[col] >> row & 1)) continue;
                const uint32_t h = hash((uint32_t)idx++ * 2654435761u);
                const int x0 = ox + col * 5 + 2, y0 = 14 + row * 5 + 2;
                const int vx = (x0 - 64) * 3 + (int)(h % 60) - 30;   // px/s
                const int vy = (y0 - 32) * 2 - 40 - (int)(h >> 8) % 70;
                const int x = x0 + vx * tau / 1000;
                const int y = y0 + vy * tau / 1000 + 90 * tau / 1000 * tau / 1000;
                const int s = tau < 300 ? 5 : tau < 700 ? 3 : 2;
                gfx_fill_rect(g, x - s / 2, y - s / 2, s, s, GFX_WHITE);
            }
        }
    }
}

void egg_render(const app_t *a, gfx_t *g) {
    const uint32_t t = a->now - a->g.egg.t0;
    // The screen shakes after each hit.
    int dx = 0, dy = 0;
    const bool shaking = (t >= T_SIX_HIT && t < T_SIX_HIT + 220) || (t >= T_SEVEN_HIT && t < T_SEVEN_HIT + 220) ||
                         (t >= T_BOOM && t < T_BOOM + 350);
    if (shaking) {
        const uint32_t h = hash(t / 30);
        dx = (int)(h % 7) - 3;
        dy = (int)(h >> 8) % 5 - 2;
    }
    gfx_set_offset(g, dx, dy);
    if (t < T_SIX) {
        draw_charge(g, t);
    } else if (t < T_SEVEN_HIT + 1) {
        draw_slam(g, t);
    } else if (t < T_MEME) {
        draw_rings(g, t);
    } else if (t < T_SPIN) {
        draw_meme(g, t);
    } else if (t < T_BOOM) {
        draw_spin(g, t);
    } else {
        draw_boom(g, t);
        if (t >= T_FINALE && (t - T_FINALE) % 400 < 300) {
            font_draw_centered(g, 64, 18, "67", 3, GFX_WHITE);
            for (int i = 0; i < 6; i++) {
                const uint32_t h = hash(t / 150 * 7 + (uint32_t)i);
                sparkle(g, 30 + (int)(h % 68), 10 + (int)(h >> 8) % 36, 1);
            }
        }
    }
    gfx_set_offset(g, 0, 0);
    // Flashes: the screen turns over on each impact.
    if ((t >= T_SIX_HIT && t < T_SIX_HIT + 70) || (t >= T_SEVEN_HIT && t < T_SEVEN_HIT + 70) ||
        (t >= T_BOOM && t < T_BOOM + 120)) {
        invert(g);
    }
}

bool egg_animating(const app_t *a) {
    (void)a;
    return true;
}
