// Battery maths with no hardware in it: millivolts to percent and level.
// Pure C (host-tested). battery.c reads the ADC and calls these.

#pragma once

#include <stdint.h>

typedef enum {
    BATTERY_UNKNOWN = 0,  // no ADC fitted (FEATURE_BATTERY_ADC off) or read failed
    BATTERY_OK,
    BATTERY_LOW,
    BATTERY_CRITICAL,
} battery_level_t;

typedef struct {
    battery_level_t level;
    int mv;       // cell voltage, -1 when unknown
    int percent;  // 0..100, -1 when unknown
} battery_status_t;

// Rough resting LiPo curve between BATTERY_MV_EMPTY and BATTERY_MV_FULL.
int battery_percent_from_mv(int mv);

// Level with hysteresis: once LOW, it needs +50 mV to return to OK, so the
// icon does not flicker as the voltage sags under the display's load.
battery_level_t battery_level_from_mv(int mv, battery_level_t prev);
