// The red LED: alarms, glitches, reactions, a flash on every note of a song
// or babble, a slow breath while petted, a stuck flicker while hung.
//
// LEDC PWM on PIN_LED from the crystal clock (its own timer, separate from the
// buzzer's). Patterns are stepped by an esp_timer; light sleep is held off
// only while one runs, and the LED is off at rest.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

typedef enum {
    LED_OFF = 0,
    LED_BLINK,   // one short blink
    LED_SIREN,   // fast red flashes, about four seconds
    LED_GLITCH,  // irregular flicker
    LED_DOUBLE,  // two blinks
    LED_GIGGLE,  // four quick blinks
    LED_SCARED,  // a fast shiver
    LED_SNEEZE,  // dark, then a flash on the "choo"
    LED_FURY,    // frantic flicker, then a hard flash
    LED_KO,      // a bright flash fading out
    LED_BOOT,    // dark, a slow rise, three blinks
    LED_BEAT,    // ba-dum (with each heartbeat sound)
    LED_BURST,   // sparkling flicker, then a glow fading out
    LED_TWINKLE, // a few soft twinkles (snow)
    LED_RISE,    // a slow glow up and out (waking)
    // Loops: until another pattern replaces them, or LED_OFF.
    LED_BREATHE,  // slow breathing (petted)
    LED_DOZE,     // slower and dimmer (dozing off)
    LED_STUCK,    // the odd stuck flicker (hung)
} led_pattern_t;

#if FEATURE_LED
void led_init(void);
void led_pattern(led_pattern_t p);  // replaces whatever is running
void led_flash(uint16_t ms);        // on for ms, then off (safe from the timer task)
bool led_busy(void);                // a pattern is running
#else
static inline void led_init(void) {}
static inline void led_pattern(led_pattern_t p) { (void)p; }
static inline void led_flash(uint16_t ms) { (void)ms; }
static inline bool led_busy(void) { return false; }
#endif
