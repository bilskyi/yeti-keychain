// A 5x7 bitmap font (digits, A-Z and a little punctuation) drawn into a gfx_t,
// scaled by an integer. Lower case is drawn as upper case; anything else as a
// space. Pure C (host-tested).

#pragma once

#include "display/gfx.h"

#define FONT_W 5
#define FONT_H 7
#define FONT_ADVANCE 6  // glyph + 1 px gap, at scale 1

// Draws s with its top-left at (x, y). Returns the width drawn.
int font_draw(gfx_t *g, int x, int y, const char *s, int scale, gfx_color_t c);
int font_width(const char *s, int scale);
// The 5 columns of a character (LSB on top); a blank for ones it lacks.
const uint8_t *font_glyph(char c);
// Centred on cx.
void font_draw_centered(gfx_t *g, int cx, int y, const char *s, int scale, gfx_color_t c);
