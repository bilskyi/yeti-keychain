#include "app/music_viz.h"

#include "app/app_internal.h"

#define W GFX_W
#define H GFX_H

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static int absi(int v) {
    return v < 0 ? -v : v;
}

static int rnd(rng_t *r, int lo, int hi) {
    return lo + (int)rng_range(r, 0, (uint32_t)(hi - lo));
}

// Sine of an angle in 1/1024 turn, x256: app_sin64 with the steps between
// interpolated, so slow turns (the record) don't jump.
static int fsin(int a) {
    a &= 1023;
    const int i = a >> 4, f = a & 15;
    return (app_sin64(i) * (16 - f) + app_sin64(i + 1) * f) / 16;
}

static int fcos(int a) {
    return fsin(a + 256);
}

// A circle outline (midpoint); dot > 1 keeps every dot-th pixel per octant.
static void ring(gfx_t *g, int cx, int cy, int r, int dot, gfx_color_t c) {
    if (r <= 0) return;
    int x = r, y = 0, err = 1 - r, k = 0;
    while (x >= y) {
        if (k++ % dot == 0) {
            gfx_pixel(g, cx + x, cy + y, c);
            gfx_pixel(g, cx + y, cy + x, c);
            gfx_pixel(g, cx - y, cy + x, c);
            gfx_pixel(g, cx - x, cy + y, c);
            gfx_pixel(g, cx - x, cy - y, c);
            gfx_pixel(g, cx - y, cy - x, c);
            gfx_pixel(g, cx + y, cy - x, c);
            gfx_pixel(g, cx + x, cy - y, c);
        }
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

// A quaver: head, stem, flag.
static void quaver(gfx_t *g, int x, int y) {
    gfx_fill_circle(g, x, y, 2, GFX_WHITE);
    gfx_fill_rect(g, x + 2, y - 7, 1, 8, GFX_WHITE);
    gfx_fill_rect(g, x + 3, y - 7, 2, 1, GFX_WHITE);
    gfx_pixel(g, x + 5, y - 6, GFX_WHITE);
}

// A free slot in a pool of little moving things (sparks, notes): the oldest
// one when all are busy.
static viz_bit_t *slot(viz_bit_t *pool, int n) {
    viz_bit_t *best = &pool[0];
    for (int i = 0; i < n; i++) {
        if (pool[i].life <= 0) return &pool[i];
        if (pool[i].life < best->life) best = &pool[i];
    }
    return best;
}

static void move_bits(viz_bit_t *pool, int n, int dt, int gravity) {
    for (int i = 0; i < n; i++) {
        viz_bit_t *b = &pool[i];
        if (b->life <= 0) continue;
        b->x = (int16_t)(b->x + b->vx * dt / 1000);
        b->y = (int16_t)(b->y + b->vy * dt / 1000);
        b->vy = (int16_t)(b->vy + gravity * dt / 1000);
        b->life = (int16_t)(b->life - dt);
    }
}

// --- the track ---

// Semitones from lo up to hz (rounded to the nearest).
static int semis(uint32_t lo, uint32_t hz) {
    uint64_t f = (uint64_t)lo << 16;
    const uint64_t target = ((uint64_t)hz << 16) * 1029 / 1000;  // a quarter tone of slack
    int k = 0;
    while (k < 96 && f * 1059463 / 1000000 <= target) {
        f = f * 1059463 / 1000000;
        k++;
    }
    return k;
}

bool viz_track_load(viz_track_t *tr, int song) {
    tr->n = rtttl_parse(song_rtttl(song), tr->notes, VIZ_TRACK_MAX, 0);
    uint32_t lo = 0, hi = 0;
    for (int i = 0; i < tr->n; i++) {
        const uint32_t hz = tr->notes[i].hz;
        if (!hz) continue;
        if (!lo || hz < lo) lo = hz;
        if (hz > hi) hi = hz;
    }
    const int top = lo ? semis(lo, hi) : 0;
    for (int i = 0; i < tr->n; i++) {
        const uint32_t hz = tr->notes[i].hz;
        tr->pitch[i] = (uint8_t)(!hz ? 0 : top ? clampi(semis(lo, hz) * 255 / top, 0, 255) : 128);
    }
    return tr->n > 0;
}

// --- start ---

void viz_start(viz_t *v, viz_kind_t kind, rng_t *rng) {
    *v = (viz_t){.kind = kind};
    if (kind == VIZ_WARP) {
        for (int i = 0; i < VIZ_STARS; i++) {
            v->s.warp.x[i] = (int16_t)rnd(rng, -1024, 1024);
            v->s.warp.y[i] = (int16_t)rnd(rng, -1024, 1024);
            v->s.warp.z[i] = (int16_t)rnd(rng, 100, 1024);
        }
        v->s.warp.v = 256;
    } else if (kind == VIZ_SCOPE) {
        v->s.scope.per = 24;
    } else if (kind == VIZ_SYNTHWAVE) {
        v->s.synth.v = 256;
    }
}

// --- notes ---

// Equalizer: how much a bar d bars away from the note's own jumps (x256).
static const int16_t EQ_SPREAD[] = {256, 209, 115, 42, 10, 2};

void viz_note(viz_t *v, const viz_track_t *tr, int i, rng_t *rng) {
    const int p = tr->pitch[i], ms = tr->notes[i].ms;
    v->notes_seen++;
    switch (v->kind) {
        case VIZ_EQ: {
            const int c = p * 15 * 16 / 255;  // the note's bar, 1/16 bar
            for (int b = 0; b < 16; b++) {
                const int d = absi(b * 16 - c);
                const int k = d / 16 < 5 ? (EQ_SPREAD[d / 16] * (16 - d % 16) + EQ_SPREAD[d / 16 + 1] * (d % 16)) / 16 : 0;
                int h = k * (34 + (ms < 400 ? ms : 400) / 18) * 16 / 256 * rnd(rng, 80, 110) / 100 + rnd(rng, 0, 96);
                h = h > 52 * 16 ? 52 * 16 : h;
                if (h > v->s.eq.h[b]) v->s.eq.h[b] = (int16_t)h;
            }
            break;
        }
        case VIZ_RAIN: {
            const int lane = p * 5 / 256;
            v->s.rain.flash[lane] = 180;
            for (int k = 0; k < 6; k++) {
                viz_bit_t *s = slot(v->s.rain.sparks, VIZ_SPARKS);
                *s = (viz_bit_t){(int16_t)((13 + lane * 25 + rnd(rng, -6, 6)) * 16), 52 * 16,
                                 (int16_t)(rnd(rng, -30, 30) * 16), (int16_t)(-rnd(rng, 25, 70) * 16),
                                 (int16_t)rnd(rng, 200, 450)};
            }
            break;
        }
        case VIZ_SCOPE:
            v->s.scope.amp = 26 * 16;
            v->s.scope.per = 44 - p * 34 / 255;
            break;
        case VIZ_WARP:
            v->s.warp.v = clampi(v->s.warp.v + 614 + p * 921 / 255, 0, 2662);
            v->s.warp.ring = 250;
            break;
        case VIZ_DJ:
            v->s.dj.bob = 4 * 16;
            if (p > 158) v->s.dj.happy = 250;
            if (v->notes_seen % 2 == 0) {
                v->s.dj.side ^= 1;
                viz_bit_t *n = slot(v->s.dj.notes, VIZ_FLOATS);
                *n = (viz_bit_t){(int16_t)((v->s.dj.side ? 112 : 10) * 16), 30 * 16,
                                 (int16_t)((v->s.dj.side ? 14 : -14) * 16), -22 * 16, 1200};
            }
            break;
        case VIZ_VINYL:
            v->s.vinyl.level = (10 + p * 22 / 255) * 16;
            if (v->notes_seen % 3 == 0) {
                *slot(v->s.vinyl.notes, VIZ_FLOATS) = (viz_bit_t){64 * 16, 22 * 16, 10 * 16, -16 * 16, 1400};
            }
            break;
        case VIZ_SYNTHWAVE:
            v->s.synth.v = 819;
            v->s.synth.twinkle[v->notes_seen % 18] = 400;
            break;
        case VIZ_DANCE:
            if (v->notes_seen % 8 == 0) v->s.dance.move = (v->s.dance.move + 1 + rnd(rng, 0, 2)) % 4;
            if (v->s.dance.move == 2) v->s.dance.jump = 250;
            if (p > 178) v->s.dance.rays = 200;
            break;
        case VIZ_BOUNCE:
            v->s.bounce.squash = 125;
            break;
        case VIZ_COUNT:
            break;
    }
}

// --- time ---

void viz_step(viz_t *v, int dt, rng_t *rng) {
    switch (v->kind) {
        case VIZ_EQ:
            for (int b = 0; b < 16; b++) {
                int h = v->s.eq.h[b] - dt * 1120 / 1000;
                v->s.eq.h[b] = (int16_t)(h < 0 ? 0 : h);
                if (v->s.eq.h[b] >= v->s.eq.pk[b]) {
                    v->s.eq.pk[b] = v->s.eq.h[b];
                    v->s.eq.hold[b] = 350;
                } else if ((v->s.eq.hold[b] = (int16_t)(v->s.eq.hold[b] - dt)) < 0) {
                    const int pk = v->s.eq.pk[b] - dt * 480 / 1000;
                    v->s.eq.pk[b] = (int16_t)(pk < 0 ? 0 : pk);
                }
            }
            break;
        case VIZ_RAIN:
            for (int l = 0; l < 5; l++) v->s.rain.flash[l] = (int16_t)(v->s.rain.flash[l] - dt);
            move_bits(v->s.rain.sparks, VIZ_SPARKS, dt, 200 * 16);
            break;
        case VIZ_SCOPE:
            v->s.scope.amp = v->s.scope.amp - dt * 480 / 1000 < 32 ? 32 : v->s.scope.amp - dt * 480 / 1000;
            v->s.scope.ph = (v->s.scope.ph + dt * 978 / 1000) & 1023;
            break;
        case VIZ_WARP: {
            const int sp = v->s.warp.v;
            for (int i = 0; i < VIZ_STARS; i++) {
                const int z = v->s.warp.z[i] - sp * dt / 1000;
                if (z < 31) {
                    v->s.warp.x[i] = (int16_t)rnd(rng, -1024, 1024);
                    v->s.warp.y[i] = (int16_t)rnd(rng, -1024, 1024);
                    v->s.warp.z[i] = 1024;
                } else {
                    v->s.warp.z[i] = (int16_t)z;
                }
            }
            const int k = dt * 25 / 10 > 1000 ? 1000 : dt * 25 / 10;
            v->s.warp.v += (225 - v->s.warp.v) * k / 1000;
            v->s.warp.ring -= dt;
            break;
        }
        case VIZ_DJ:
            v->s.dj.bob = v->s.dj.bob - dt * 352 / 1000 < 0 ? 0 : v->s.dj.bob - dt * 352 / 1000;
            v->s.dj.happy -= dt;
            move_bits(v->s.dj.notes, VIZ_FLOATS, dt, 0);
            break;
        case VIZ_VINYL:
            v->s.vinyl.angle = (v->s.vinyl.angle + dt * 563 / 1000) & 1023;
            v->s.vinyl.level = v->s.vinyl.level - dt * 720 / 1000 < 0 ? 0 : v->s.vinyl.level - dt * 720 / 1000;
            move_bits(v->s.vinyl.notes, VIZ_FLOATS, dt, 0);
            break;
        case VIZ_SYNTHWAVE: {
            const int k = dt * 3 > 1000 ? 1000 : dt * 3;
            v->s.synth.v += (256 - v->s.synth.v) * k / 1000;
            v->s.synth.z = (v->s.synth.z + dt * v->s.synth.v * 9 / 10000) & 255;
            for (int i = 0; i < 18; i++) v->s.synth.twinkle[i] = (int16_t)(v->s.synth.twinkle[i] - dt);
            break;
        }
        case VIZ_DANCE:
            v->s.dance.jump = v->s.dance.jump - dt < 0 ? 0 : v->s.dance.jump - dt;
            v->s.dance.rays -= dt;
            break;
        case VIZ_BOUNCE:
            v->s.bounce.squash -= dt;
            break;
        case VIZ_COUNT:
            break;
    }
}

// --- drawing ---

static void draw_eq(const viz_t *v, gfx_t *g) {
    for (int b = 0; b < 16; b++) {
        const int x = b * 8 + 1, segs = v->s.eq.h[b] / 48;
        gfx_hline(g, x, x + 5, 63, GFX_WHITE);  // the floor, so a rest isn't a black screen
        for (int k = 0; k < segs; k++) gfx_fill_rect(g, x, 61 - k * 3, 6, 2, GFX_WHITE);
        if (v->s.eq.pk[b] > 16) gfx_fill_rect(g, x, 61 - v->s.eq.pk[b] / 48 * 3 - 2, 6, 1, GFX_WHITE);
    }
}

#define RAIN_AHEAD 1400
#define RAIN_HIT 53

static void draw_rain(const viz_t *v, const viz_track_t *tr, uint32_t t, gfx_t *g) {
    for (int l = 0; l < 5; l++) {
        for (int y = 0; y < RAIN_HIT; y += 4) gfx_pixel(g, l * 25 + 13, y, GFX_WHITE);
    }
    uint32_t start = 0;
    for (int i = 0; i < tr->n; start += tr->notes[i].ms, i++) {
        if (!tr->notes[i].hz) continue;
        const int ahead = (int)start - (int)t;
        if (ahead > RAIN_AHEAD) break;
        if ((int)(start + tr->notes[i].ms) < (int)t - 60) continue;
        const int x = tr->pitch[i] * 5 / 256 * 25 + 3;
        const int len = clampi(tr->notes[i].ms / 30, 3, 14);
        if (ahead > 0) {
            const int y = RAIN_HIT - ahead * RAIN_HIT / RAIN_AHEAD;
            gfx_fill_rect(g, x + 2, y - len, 17, len, GFX_WHITE);
        } else {
            gfx_rect(g, x + 2, RAIN_HIT - len, 17, len, GFX_WHITE);
        }
    }
    for (int l = 0; l < 5; l++) {
        const int x = l * 25 + 3;
        if (v->s.rain.flash[l] > 0) {
            gfx_fill_rect(g, x, RAIN_HIT, 21, 5, GFX_WHITE);
            gfx_fill_rect(g, x + 4, RAIN_HIT + 6, 13, 2, GFX_WHITE);
        } else {
            gfx_rect(g, x, RAIN_HIT, 21, 5, GFX_WHITE);
        }
    }
    for (int i = 0; i < VIZ_SPARKS; i++) {
        const viz_bit_t *s = &v->s.rain.sparks[i];
        if (s->life > 0) gfx_pixel(g, s->x / 16, s->y / 16, GFX_WHITE);
    }
    if (v->notes_seen > 1) {
        char b[8] = "X";
        app_itoa(v->notes_seen, b + 1);
        font_draw(g, W - font_width(b, 1), 0, b, 1, GFX_WHITE);
    }
}

static int scope_y(const viz_t *v, int x, int ph, int amp) {
    int s = fsin(x * 1024 / v->s.scope.per - ph) * 16 / 10;  // squashed toward a square
    s = clampi(s, -256, 256);
    const int env = 154 + 102 * fsin(x * 512 / W) / 256;  // quieter at the edges
    return 32 + s * amp / 256 * env / 256 / 16;
}

static void draw_scope(const viz_t *v, gfx_t *g) {
    for (int x = 0; x < W; x += 16)
        for (int y = 0; y < H; y += 4) gfx_pixel(g, x, y, GFX_WHITE);
    for (int y = 0; y < H; y += 16)
        for (int x = 0; x < W; x += 4) gfx_pixel(g, x, y, GFX_WHITE);
    // The afterglow: the wave a moment ago, dotted.
    for (int x = 0; x < W; x += 2) gfx_pixel(g, x, scope_y(v, x, v->s.scope.ph - 80, v->s.scope.amp * 9 / 8), GFX_WHITE);
    int prev = scope_y(v, 0, v->s.scope.ph, v->s.scope.amp);
    for (int x = 1; x < W; x++) {
        const int y = scope_y(v, x, v->s.scope.ph, v->s.scope.amp);
        gfx_line(g, x - 1, prev, x, y, GFX_WHITE);
        gfx_pixel(g, x, y + 1, GFX_WHITE);
        prev = y;
    }
}

static void draw_warp(const viz_t *v, gfx_t *g) {
    const int sp = v->s.warp.v;
    for (int i = 0; i < VIZ_STARS; i++) {
        const int x = v->s.warp.x[i], y = v->s.warp.y[i], z = v->s.warp.z[i];
        const int X = 64 + x * 40 / z, Y = 32 + y * 40 / z;
        if (X < -2 || X > W + 2 || Y < -2 || Y > H + 2) continue;
        if (sp > 614) {
            const int z0 = z + sp * 40 / 1000;
            gfx_line(g, 64 + x * 40 / z0, 32 + y * 40 / z0, X, Y, GFX_WHITE);
        } else {
            gfx_pixel(g, X, Y, GFX_WHITE);
        }
        if (z < 256) gfx_pixel(g, X + 1, Y, GFX_WHITE);
    }
    if (v->s.warp.ring > 0) ring(g, 64, 32, (250 - v->s.warp.ring) * 22 / 250 + 2, 2, GFX_WHITE);
}

static void draw_dj(const viz_t *v, gfx_t *g) {
    const int b = v->s.dj.bob / 16;
    // Headphones: the band over the top, a cup each side.
    for (int r = 0; r < 2; r++) {
        int px = 64 - (46 - r), py = 34 + b;
        for (int k = 1; k <= 32; k++) {
            const int a = 512 + k * 16;
            const int x = 64 + fcos(a) * (46 - r) / 256, y = 34 + b + fsin(a) * (30 - r) / 256;
            gfx_line(g, px, py, x, y, GFX_WHITE);
            px = x;
            py = y;
        }
    }
    gfx_fill_rrect(g, 12, 26 + b, 9, 20, 2, GFX_WHITE);
    gfx_fill_rrect(g, 107, 26 + b, 9, 20, 2, GFX_WHITE);
    gfx_fill_rect(g, 10, 29 + b, 2, 14, GFX_WHITE);
    gfx_fill_rect(g, 116, 29 + b, 2, 14, GFX_WHITE);
    for (int e = 0; e < 2; e++) {
        const int ex = e ? 82 : 46;
        if (v->s.dj.happy > 0) {  // squeezed shut with joy: ^ ^
            for (int k = 0; k < 3; k++) {
                gfx_line(g, ex - 8, 34 + b + k, ex, 26 + b + k, GFX_WHITE);
                gfx_line(g, ex, 26 + b + k, ex + 8, 34 + b + k, GFX_WHITE);
            }
        } else {
            gfx_fill_ellipse(g, ex, 30 + b, 7, 9, GFX_WHITE);
            gfx_fill_circle(g, ex + 2, 26 + b, 2, GFX_BLACK);
        }
    }
    if (v->s.dj.bob > 2 * 16) {
        gfx_fill_ellipse(g, 64, 46 + b, 5, 3, GFX_WHITE);
    } else {
        gfx_fill_rect(g, 59, 45 + b, 11, 2, GFX_WHITE);
    }
    for (int i = 0; i < VIZ_FLOATS; i++) {
        const viz_bit_t *n = &v->s.dj.notes[i];
        if (n->life > 0) quaver(g, n->x / 16, n->y / 16);
    }
}

static void draw_vinyl(const viz_t *v, uint32_t t, gfx_t *g) {
    const int cx = 38, cy = 32, a = v->s.vinyl.angle;
    gfx_fill_circle(g, cx, cy, 29, GFX_WHITE);
    ring(g, cx, cy, 25, 1, GFX_BLACK);
    ring(g, cx, cy, 21, 1, GFX_BLACK);
    ring(g, cx, cy, 17, 1, GFX_BLACK);
    for (int k = -6; k <= 6; k++) {  // a shine that turns with the record
        const int s = a + k * 5;
        gfx_pixel(g, cx + fcos(s) * 27 / 256, cy + fsin(s) * 27 / 256, GFX_BLACK);
        gfx_pixel(g, cx + fcos(s) * 23 / 256, cy + fsin(s) * 23 / 256, GFX_BLACK);
    }
    gfx_fill_circle(g, cx, cy, 8, GFX_BLACK);
    ring(g, cx, cy, 7, 1, GFX_WHITE);
    gfx_fill_circle(g, cx, cy, 1, GFX_WHITE);
    gfx_line(g, cx + fcos(a) * 3 / 256, cy + fsin(a) * 3 / 256, cx + fcos(a) * 6 / 256, cy + fsin(a) * 6 / 256, GFX_WHITE);
    // The tonearm, wobbling a little as the record turns.
    const int w = fsin((int)(t * 1024 / 565)) > 128 ? 1 : 0;
    gfx_fill_circle(g, 86, 8, 4, GFX_WHITE);
    gfx_fill_circle(g, 86, 8, 1, GFX_BLACK);
    gfx_line(g, 86, 8, 80, 30 + w, GFX_WHITE);
    gfx_line(g, 87, 8, 81, 30 + w, GFX_WHITE);
    gfx_line(g, 80, 30 + w, 64, 22 + w, GFX_WHITE);
    gfx_line(g, 81, 31 + w, 65, 23 + w, GFX_WHITE);
    gfx_fill_rect(g, 61, 20 + w, 4, 4, GFX_WHITE);
    for (int k = 0; k < 8; k++) {  // the level meter
        if (v->s.vinyl.level > k * 64) {
            gfx_fill_rect(g, 104, 58 - k * 7, 18, 5, GFX_WHITE);
        } else {
            gfx_rect(g, 104, 58 - k * 7, 18, 5, GFX_WHITE);
        }
    }
    for (int i = 0; i < VIZ_FLOATS; i++) {
        const viz_bit_t *n = &v->s.vinyl.notes[i];
        if (n->life > 300 || (n->life > 0 && (n->life / 60) % 2)) quaver(g, n->x / 16, n->y / 16);
    }
}

static const uint8_t SKY[18][2] = {
    {6, 4},   {18, 14}, {27, 6},  {39, 20}, {47, 3},  {58, 11}, {70, 2},  {81, 16}, {90, 7},
    {101, 21}, {109, 4}, {121, 12}, {12, 23}, {33, 13}, {76, 9},  {96, 1},  {116, 24}, {52, 22},
};

#define HORIZON 38

static void draw_synthwave(const viz_t *v, uint32_t t, gfx_t *g) {
    for (int i = 0; i < 18; i++) {
        const int x = SKY[i][0], y = SKY[i][1];
        if (v->s.synth.twinkle[i] > 0) {
            gfx_hline(g, x - 1, x + 1, y, GFX_WHITE);
            gfx_pixel(g, x, y - 1, GFX_WHITE);
            gfx_pixel(g, x, y + 1, GFX_WHITE);
        } else if ((x + y) % 3 == 0) {
            gfx_pixel(g, x, y, GFX_WHITE);
        }
    }
    // The sun, its lower half cut by stripes that slide down.
    for (int y = -22; y <= 0; y++) {
        const int half = gfx_isqrt(22 * 22 - y * y);
        const int band = (y + 22 + (int)(t / 120)) % 6;
        if (y > -12 && band < (y + 12) / 4) continue;
        gfx_hline(g, 64 - half, 64 + half, HORIZON + y - 1, GFX_WHITE);
    }
    static const int8_t HILLS[2][5][2] = {
        {{0, 34}, {14, 27}, {24, 33}, {34, 29}, {44, 37}},
        {{84, 37}, {96, 28}, {108, 33}, {118, 26}, {127, 34}},
    };
    for (int m = 0; m < 2; m++)
        for (int i = 1; i < 5; i++)
            gfx_line(g, HILLS[m][i - 1][0], HILLS[m][i - 1][1], HILLS[m][i][0], HILLS[m][i][1], GFX_WHITE);
    gfx_hline(g, 0, W - 1, HORIZON, GFX_WHITE);
    for (int k = 0; k < 7; k++) {  // the grid rushing toward you
        const int z = (k * 256 + v->s.synth.z) / 7;
        gfx_hline(g, 0, W - 1, HORIZON + 1 + (H - HORIZON) * z * z / 65536, GFX_WHITE);
    }
    for (int k = -9; k <= 9; k++) gfx_line(g, 64 + k * 6, HORIZON, 64 + k * 30, H - 1, GFX_WHITE);
}

static void dance_arm(const viz_t *v, gfx_t *g, int sx, int cy, int side, int ph) {
    const int up = ph * side > 0;
    int ex, ey;
    switch (v->s.dance.move) {
        case 0: ex = sx + side * 9; ey = cy - 14 + (up ? -4 : 2); break;     // waving
        case 1: ex = sx + side * 12; ey = cy + (up ? -8 : 6); break;         // swaying
        case 2: ex = sx + side * 10; ey = cy - 16; break;                    // jumping, arms up
        default: ex = sx + side * (up ? 14 : 6); ey = cy - 2 + (up ? -10 : 4); break;  // the robot
    }
    for (int k = -1; k <= 1; k++) gfx_line(g, sx, cy - 2 + k, ex, ey + k, GFX_WHITE);
    gfx_fill_circle(g, ex, ey, 2, GFX_WHITE);
}

static void draw_dance(const viz_t *v, uint32_t t, gfx_t *g) {
    // The disco ball, its tiles turning.
    gfx_line(g, 64, 0, 64, 3, GFX_WHITE);
    for (int y = -6; y <= 6; y++)
        for (int x = -6; x <= 6; x++)
            if (x * x + y * y <= 40 && (((x + 8 + (int)(t / 150)) / 2 + (y + 8) / 2) & 1))
                gfx_pixel(g, 64 + x, 10 + y, GFX_WHITE);
    ring(g, 64, 10, 7, 1, GFX_WHITE);
    if (v->s.dance.rays > 0) {
        for (int k = 0; k < 6; k++) {
            const int a = 77 + k * 72;
            gfx_line(g, 64 + fcos(a) * 9 / 256, 10 + fsin(a) * 9 / 256, 64 + fcos(a) * 30 / 256, 10 + fsin(a) * 30 / 256, GFX_WHITE);
        }
    }
    const int ph = v->notes_seen % 2 ? 1 : -1;
    const int jy = -(fsin((250 - v->s.dance.jump) * 512 / 250) * 6 / 256);
    const int sway = v->s.dance.move == 1 ? ph * 5 : 0;
    const int cx = 64 + sway, cy = 42 + jy;
    for (int x = 20; x < 108; x += 2) gfx_pixel(g, x, 61, GFX_WHITE);
    gfx_fill_ellipse(g, 64 + sway, 61, 12 + jy, 1, GFX_WHITE);
    const int kick = v->s.dance.move == 3 ? ph * 3 : 0;
    gfx_fill_rect(g, cx - 8 + kick, cy + 9, 5, 9, GFX_WHITE);
    gfx_fill_rect(g, cx + 3 - kick, cy + 9, 5, 9, GFX_WHITE);
    gfx_fill_ellipse(g, cx, cy, 15, 13, GFX_WHITE);
    gfx_fill_ellipse(g, cx, cy - 2, 9, 7, GFX_BLACK);
    gfx_fill_circle(g, cx - 4, cy - 3, 2, GFX_WHITE);
    gfx_fill_circle(g, cx + 4, cy - 3, 2, GFX_WHITE);
    if (v->notes_seen % 4 < 2) {
        gfx_fill_rect(g, cx - 2, cy + 2, 5, 2, GFX_WHITE);
    } else {
        gfx_fill_ellipse(g, cx, cy + 3, 2, 2, GFX_WHITE);
    }
    dance_arm(v, g, cx - 14, cy, -1, ph);
    dance_arm(v, g, cx + 14, cy, 1, ph);
}

#define BALL_X 28

static int step_y(const viz_track_t *tr, int i) {
    return 54 - tr->pitch[i] * 40 / 255;
}

static void draw_bounce(const viz_t *v, const viz_track_t *tr, uint32_t t, gfx_t *g) {
    int cur = -1, nxt = -1;
    uint32_t cur_at = 0, nxt_at = 0, start = 0;
    for (int i = 0; i < tr->n; start += tr->notes[i].ms, i++) {
        if (!tr->notes[i].hz) continue;
        const int x = BALL_X + ((int)start - (int)t) * 55 / 1000;
        if (x > W + 10) break;
        const int w = clampi(tr->notes[i].ms * 55 / 1000 - 2, 3, W);
        if (x + w >= -2) gfx_fill_rect(g, x, step_y(tr, i), w, 2, GFX_WHITE);
        if (start <= t) {
            cur = i;
            cur_at = start;
        } else if (nxt < 0) {
            nxt = i;
            nxt_at = start;
        }
    }
    int by = 30;
    if (cur >= 0 && nxt >= 0) {
        // A parabola from this note's step to the next one's, timed to land on it.
        const int span = (int)(nxt_at - cur_at);
        const int k = clampi((int)(t - cur_at) * 256 / (span ? span : 1), 0, 256);
        const int y0 = step_y(tr, cur) - 4, y1 = step_y(tr, nxt) - 4;
        const int peak = (y0 < y1 ? y0 : y1) - 6 - span / 40;
        by = ((256 - k) * (256 - k) * y0 + 2 * (256 - k) * k * peak + k * k * y1) / 65536;
    } else if (cur >= 0) {
        by = step_y(tr, cur) - 4;
    }
    const int sq = v->s.bounce.squash > 60 ? 1 : 0;
    gfx_fill_ellipse(g, BALL_X, by + sq, 3 + sq, 3 - sq, GFX_WHITE);
    for (int y = 0; y < H; y += 3) gfx_pixel(g, BALL_X, y, GFX_WHITE);
}

void viz_draw(const viz_t *v, const viz_track_t *tr, uint32_t t, gfx_t *g) {
    switch (v->kind) {
        case VIZ_EQ: draw_eq(v, g); break;
        case VIZ_RAIN: draw_rain(v, tr, t, g); break;
        case VIZ_SCOPE: draw_scope(v, g); break;
        case VIZ_WARP: draw_warp(v, g); break;
        case VIZ_DJ: draw_dj(v, g); break;
        case VIZ_VINYL: draw_vinyl(v, t, g); break;
        case VIZ_SYNTHWAVE: draw_synthwave(v, t, g); break;
        case VIZ_DANCE: draw_dance(v, t, g); break;
        case VIZ_BOUNCE: draw_bounce(v, tr, t, g); break;
        case VIZ_COUNT: break;
    }
}

const char *viz_name(viz_kind_t kind) {
    static const char *const NAMES[VIZ_COUNT] = {
        "DANCE", "BOUNCE", "SYNTHWAVE", "VINYL", "DJ", "WARP", "NOTE RAIN", "SCOPE", "EQUALIZER",
    };
    return (unsigned)kind < VIZ_COUNT ? NAMES[kind] : "?";
}
