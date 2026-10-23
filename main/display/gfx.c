#include "gfx.h"

#include <stdlib.h>
#include <string.h>

void gfx_clear(gfx_t *g) {
    memset(g->buf, 0, sizeof(g->buf));
}

void gfx_set_offset(gfx_t *g, int ox, int oy) {
    g->ox = ox;
    g->oy = oy;
}

int gfx_get_pixel(const gfx_t *g, int x, int y) {
    if (x < 0 || x >= GFX_W || y < 0 || y >= GFX_H) return 0;
    return (g->buf[(y >> 3) * GFX_W + x] >> (y & 7)) & 1;
}

// Screen coordinates, already offset.
static void put(gfx_t *g, int x, int y, gfx_color_t c) {
    if (x < 0 || x >= GFX_W || y < 0 || y >= GFX_H) return;
    uint8_t *b = &g->buf[(y >> 3) * GFX_W + x];
    const uint8_t bit = (uint8_t)(1u << (y & 7));
    if (c == GFX_WHITE) {
        *b |= bit;
    } else {
        *b &= (uint8_t)~bit;
    }
}

// Screen coordinates, already offset. Clips once, then runs the span.
static void span(gfx_t *g, int x0, int x1, int y, gfx_color_t c) {
    if (y < 0 || y >= GFX_H) return;
    if (x0 > x1) {
        const int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (x1 < 0 || x0 >= GFX_W) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= GFX_W) x1 = GFX_W - 1;
    for (int x = x0; x <= x1; x++) put(g, x, y, c);
}

void gfx_pixel(gfx_t *g, int x, int y, gfx_color_t c) {
    put(g, x + g->ox, y + g->oy, c);
}

void gfx_hline(gfx_t *g, int x0, int x1, int y, gfx_color_t c) {
    span(g, x0 + g->ox, x1 + g->ox, y + g->oy, c);
}

void gfx_line(gfx_t *g, int x0, int y0, int x1, int y1, gfx_color_t c) {
    // Bresenham.
    const int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gfx_pixel(g, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_fill_rect(gfx_t *g, int x, int y, int w, int h, gfx_color_t c) {
    for (int row = 0; row < h; row++) gfx_hline(g, x, x + w - 1, y + row, c);
}

void gfx_rect(gfx_t *g, int x, int y, int w, int h, gfx_color_t c) {
    if (w <= 0 || h <= 0) return;
    gfx_hline(g, x, x + w - 1, y, c);
    gfx_hline(g, x, x + w - 1, y + h - 1, c);
    for (int row = 1; row < h - 1; row++) {
        gfx_pixel(g, x, y + row, c);
        gfx_pixel(g, x + w - 1, y + row, c);
    }
}

int gfx_isqrt(int v) {
    if (v <= 0) return 0;
    int r = 0;
    int bit = 1 << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

void gfx_fill_rrect(gfx_t *g, int x, int y, int w, int h, int r, gfx_color_t c) {
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r < 0) r = 0;
    for (int row = 0; row < h; row++) {
        // How far into a corner's arc this row is, measured from the arc's
        // centre line; 0 for the straight middle section.
        int d = 0;
        if (row < r) d = r - row;
        else if (row > h - 1 - r) d = row - (h - 1 - r);
        const int inset = d ? r - gfx_isqrt(r * r - d * d) : 0;
        gfx_hline(g, x + inset, x + w - 1 - inset, y + row, c);
    }
}

void gfx_fill_circle(gfx_t *g, int cx, int cy, int r, gfx_color_t c) {
    gfx_fill_ellipse(g, cx, cy, r, r, c);
}

void gfx_fill_ellipse(gfx_t *g, int cx, int cy, int rx, int ry, gfx_color_t c) {
    if (rx < 0 || ry < 0) return;
    if (ry == 0) {
        gfx_hline(g, cx - rx, cx + rx, cy, c);
        return;
    }
    // Half-width of each row: rx * sqrt(1 - dy^2 / (ry + 1/2)^2), in integers
    // (everything doubled to keep the half). Measuring against ry + 1/2
    // rather than ry stops the top and bottom rows collapsing to a single
    // pixel, which made every small ellipse grow a spike.
    const int d = 2 * ry + 1;
    for (int dy = -ry; dy <= ry; dy++) {
        const int hw = gfx_isqrt(rx * rx * (d * d - 4 * dy * dy) / (d * d));
        gfx_hline(g, cx - hw, cx + hw, cy + dy, c);
    }
}

void gfx_fill_triangle(gfx_t *g, int x0, int y0, int x1, int y1, int x2, int y2,
                       gfx_color_t c) {
    // Sort by y, then fill each row between the long edge and the short ones.
    int t;
#define SWAP(a, b) (t = (a), (a) = (b), (b) = t)
    if (y0 > y1) { SWAP(x0, x1); SWAP(y0, y1); }
    if (y1 > y2) { SWAP(x1, x2); SWAP(y1, y2); }
    if (y0 > y1) { SWAP(x0, x1); SWAP(y0, y1); }
#undef SWAP
    if (y0 == y2) {
        int lo = x0, hi = x0;
        if (x1 < lo) lo = x1;
        if (x1 > hi) hi = x1;
        if (x2 < lo) lo = x2;
        if (x2 > hi) hi = x2;
        gfx_hline(g, lo, hi, y0, c);
        return;
    }
    for (int y = y0; y <= y2; y++) {
        const int xa = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        int xb;
        if (y < y1 || y1 == y2) {
            xb = (y1 == y0) ? x1 : x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        } else {
            xb = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
        }
        gfx_hline(g, xa, xb, y, c);
    }
}
