// Overlays drawn on top of the face: for now just the battery icon.
// Pure C (host-tested).

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "display/gfx.h"
#include "power/battery_logic.h"

// Small battery in the top-left corner. Draws nothing when the level is
// BATTERY_UNKNOWN (no ADC fitted, or on USB) unless charging.
// `charging`: the charger's cable is in. A bolt beside it, and the fill
// sweeps up to full until the cell is full.
void hud_battery(gfx_t *g, const battery_status_t *b, bool charging, uint32_t now_ms);

// The sound is off: a crossed-out speaker in the top-right corner.
void hud_muted(gfx_t *g);
