// Overlays drawn on top of the face: for now just the battery icon.
// Pure C (host-tested).

#pragma once

#include <stdint.h>

#include "display/gfx.h"
#include "power/battery_logic.h"

// Small battery in the top-left corner. Draws nothing when the level is
// BATTERY_UNKNOWN (no ADC fitted), so v0.1 shows no icon at all.
void hud_battery(gfx_t *g, const battery_status_t *b, uint32_t now_ms);
