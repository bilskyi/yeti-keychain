#include "face/hud.h"

// Inset by the burn-in shift so the icon is never clipped.
#define BAT_X 3
#define BAT_Y 3
#define BAT_W 12  // body, without the nub
#define BAT_H 6

void hud_battery(gfx_t *g, const battery_status_t *b, uint32_t now_ms) {
    if (b->level == BATTERY_UNKNOWN) return;
    // Low: blink at 1 Hz to draw the eye. Costs one small redraw per second.
    if (b->level != BATTERY_OK && (now_ms / 500) % 2) return;

    gfx_rect(g, BAT_X, BAT_Y, BAT_W, BAT_H, GFX_WHITE);
    gfx_fill_rect(g, BAT_X + BAT_W, BAT_Y + 2, 1, BAT_H - 4, GFX_WHITE);

    const int pct = b->percent < 0 ? 0 : b->percent > 100 ? 100 : b->percent;
    const int fill = (BAT_W - 4) * pct / 100;
    if (fill > 0) gfx_fill_rect(g, BAT_X + 2, BAT_Y + 2, fill, BAT_H - 4, GFX_WHITE);
}
