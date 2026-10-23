// Hardware side of power: CPU frequency + automatic light sleep, the reset
// reason, and deep sleep. The decisions (night, burn-in) are in policy.h.

#pragma once

#include <stdbool.h>
#include <stdint.h>

// 80 MHz max, XTAL min, automatic light sleep whenever every task is blocked.
void power_init(void);

// True when this boot followed a brownout reset: the battery sagged below the
// detector threshold, almost always because it is nearly empty.
bool power_reset_was_brownout(void);

// Panel off, input wake sources armed (none in v0.1), then deep sleep.
// timer_wake_ms > 0 also wakes (= reboots) after that long; 0 = no timer.
void power_deep_sleep(uint32_t timer_wake_ms) __attribute__((noreturn));
