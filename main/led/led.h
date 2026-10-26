// The red LED: alarms, glitches, and a flash on every note of a song.
//
// LEDC PWM on PIN_LED from the crystal clock (its own timer, separate from the
// buzzer's). Patterns are stepped by an esp_timer; light sleep is held off
// only while one runs, and the LED is off at rest.

#pragma once

#include <stdint.h>

#include "config.h"

typedef enum {
    LED_OFF = 0,
    LED_BLINK,   // one short blink
    LED_SIREN,   // fast red flashes, about four seconds
    LED_GLITCH,  // irregular flicker
} led_pattern_t;

#if FEATURE_LED
void led_init(void);
void led_pattern(led_pattern_t p);  // replaces whatever is running
void led_flash(uint16_t ms);        // on for ms, then off (safe from the timer task)
#else
static inline void led_init(void) {}
static inline void led_pattern(led_pattern_t p) { (void)p; }
static inline void led_flash(uint16_t ms) { (void)ms; }
#endif
