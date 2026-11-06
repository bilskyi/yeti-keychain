#include "face/hud.h"

// Inset by the burn-in shift so the icon is never clipped.
#define BAT_X 3
#define BAT_Y 3
#define BAT_W 12  // body, without the nub
#define BAT_H 6
#define BOLT_X (BAT_X + BAT_W + 3)  // the charging bolt, right of the nub

#define FILL_W (BAT_W - 4)
#define CHARGE_STEP_MS 350  // the charging sweep grows by a pixel this often

// A 4 x 6 lightning bolt.
static void bolt(gfx_t *g, int x, int y) {
    gfx_fill_rect(g, x + 2, y, 2, 1, GFX_WHITE);
    gfx_fill_rect(g, x + 1, y + 1, 2, 1, GFX_WHITE);
    gfx_fill_rect(g, x, y + 2, 4, 2, GFX_WHITE);
    gfx_fill_rect(g, x + 1, y + 4, 2, 1, GFX_WHITE);
    gfx_fill_rect(g, x, y + 5, 2, 1, GFX_WHITE);
}

void hud_battery(gfx_t *g, const battery_status_t *b, bool charging, uint32_t now_ms) {
    if (b->level == BATTERY_UNKNOWN && !charging) return;
    // Low: blink at 1 Hz to draw the eye. Costs one small redraw per second.
    // Not while charging: help is on its way.
    if (!charging && b->level != BATTERY_OK && (now_ms / 500) % 2) return;

    gfx_rect(g, BAT_X, BAT_Y, BAT_W, BAT_H, GFX_WHITE);
    gfx_fill_rect(g, BAT_X + BAT_W, BAT_Y + 2, 1, BAT_H - 4, GFX_WHITE);

    const int pct = b->percent < 0 ? 0 : b->percent > 100 ? 100 : b->percent;
    int fill = FILL_W * pct / 100;
    if (charging) {
        bolt(g, BOLT_X, BAT_Y);
        // Charging: the fill sweeps from the charge so far up to full, again
        // and again, like a phone's. Full: it stays full.
        if (pct < 100) {
            const int steps = FILL_W - fill + 2;  // the top, held for a step
            const int k = (int)((now_ms / CHARGE_STEP_MS) % (uint32_t)steps);
            fill += k < FILL_W - fill ? k : FILL_W - fill;
        }
    }
    if (fill > 0) gfx_fill_rect(g, BAT_X + 2, BAT_Y + 2, fill, BAT_H - 4, GFX_WHITE);
}

// Mirrors the battery: inset by the burn-in shift, same height.
#define MUTE_X (GFX_W - 3 - 12)

void hud_muted(gfx_t *g) {
    gfx_fill_rect(g, MUTE_X, BAT_Y + 2, 2, 3, GFX_WHITE);
    gfx_fill_triangle(g, MUTE_X + 1, BAT_Y + 3, MUTE_X + 5, BAT_Y, MUTE_X + 5, BAT_Y + 6, GFX_WHITE);
    gfx_line(g, MUTE_X + 7, BAT_Y + 1, MUTE_X + 11, BAT_Y + 5, GFX_WHITE);
    gfx_line(g, MUTE_X + 7, BAT_Y + 5, MUTE_X + 11, BAT_Y + 1, GFX_WHITE);
}
