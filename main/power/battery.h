// Battery monitoring. With FEATURE_BATTERY_ADC off every read returns
// BATTERY_UNKNOWN and the HUD hides the icon. Powered from the Super Mini's
// USB (switch off) it reads above BATTERY_MV_USB: also BATTERY_UNKNOWN.

#pragma once

#include <stdbool.h>

#include "power/battery_logic.h"

void battery_init(void);
// mv -1 when the ADC is busy (the mic holds ADC1 while it listens): pause the
// mic and read again.
battery_status_t battery_read(void);
// The TP4056's charging cable is plugged in (FEATURE_CHARGE_SENSE).
bool battery_cable_in(void);
