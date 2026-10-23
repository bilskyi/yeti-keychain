// A 128x64 1-bit framebuffer and the handful of filled shapes a face needs.
//
// Pure C, no ESP-IDF: the host tests and the preview renderer link it too.
//
// The buffer is in the panel's native page layout (8 rows per byte, LSB on
// top), so the display driver sends it as-is with no conversion.
//
// Every draw call is offset by (ox, oy) and clipped to the screen. The offset
// is how the burn-in shift moves the whole face without the face knowing.

#pragma once

#include <stdint.h>

#define GFX_W 128
#define GFX_H 64
#define GFX_PAGES (GFX_H / 8)
#define GFX_FB_BYTES (GFX_W * GFX_PAGES)

typedef enum {
    GFX_BLACK = 0,
    GFX_WHITE = 1,
} gfx_color_t;

typedef struct {
    uint8_t buf[GFX_FB_BYTES];
    int ox, oy;  // added to every coordinate
} gfx_t;

void gfx_clear(gfx_t *g);
void gfx_set_offset(gfx_t *g, int ox, int oy);

int gfx_get_pixel(const gfx_t *g, int x, int y);  // raw screen coords, no offset
void gfx_pixel(gfx_t *g, int x, int y, gfx_color_t c);
void gfx_hline(gfx_t *g, int x0, int x1, int y, gfx_color_t c);
void gfx_line(gfx_t *g, int x0, int y0, int x1, int y1, gfx_color_t c);
void gfx_fill_rect(gfx_t *g, int x, int y, int w, int h, gfx_color_t c);
void gfx_rect(gfx_t *g, int x, int y, int w, int h, gfx_color_t c);  // outline
// Rounded rectangle; r is clamped to half the shorter side.
void gfx_fill_rrect(gfx_t *g, int x, int y, int w, int h, int r, gfx_color_t c);
void gfx_fill_circle(gfx_t *g, int cx, int cy, int r, gfx_color_t c);
void gfx_fill_ellipse(gfx_t *g, int cx, int cy, int rx, int ry, gfx_color_t c);
void gfx_fill_triangle(gfx_t *g, int x0, int y0, int x1, int y1, int x2, int y2,
                       gfx_color_t c);

// Integer square root, floor. Exposed for the face geometry.
int gfx_isqrt(int v);
