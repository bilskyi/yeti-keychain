// Battery monitoring. With FEATURE_BATTERY_ADC off (v0.1) every read returns
// BATTERY_UNKNOWN and the HUD hides the icon.

#pragma once

#include "power/battery_logic.h"

void battery_init(void);
battery_status_t battery_read(void);
