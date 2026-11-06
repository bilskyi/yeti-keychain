#include "power/battery_logic.h"

#include "config.h"

#define HYSTERESIS_MV 50

int battery_percent_from_mv(int mv) {
    // Typical single-cell LiPo open-circuit curve, as (mV, %) breakpoints.
    // Generic, not measured on Sasha's cell: good to +-10 %, which is all an
    // icon needs. The ends are pinned to BATTERY_MV_EMPTY / _FULL.
    static const int16_t CURVE[][2] = {
        {BATTERY_MV_EMPTY, 0}, {3600, 10}, {3700, 30}, {3750, 45}, {3800, 55},
        {3850, 65}, {3950, 80}, {4050, 90}, {BATTERY_MV_FULL, 100},
    };
    const int n = (int)(sizeof(CURVE) / sizeof(CURVE[0]));
    if (mv <= CURVE[0][0]) return 0;
    if (mv >= CURVE[n - 1][0]) return 100;
    for (int i = 1; i < n; i++) {
        if (mv < CURVE[i][0]) {
            const int x0 = CURVE[i - 1][0], y0 = CURVE[i - 1][1];
            const int x1 = CURVE[i][0], y1 = CURVE[i][1];
            return y0 + (y1 - y0) * (mv - x0) / (x1 - x0);
        }
    }
    return 100;
}

battery_level_t battery_level_from_mv(int mv, battery_level_t prev) {
    if (mv < 0 || mv > BATTERY_MV_USB) return BATTERY_UNKNOWN;  // no reading, or on USB
    if (mv <= BATTERY_MV_CRITICAL) return BATTERY_CRITICAL;
    const int low = BATTERY_MV_LOW + ((prev == BATTERY_LOW || prev == BATTERY_CRITICAL)
                                          ? HYSTERESIS_MV : 0);
    return mv <= low ? BATTERY_LOW : BATTERY_OK;
}
