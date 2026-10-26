// Passive buzzer: short chirps that go with the face's reactions.
//
// LEDC PWM on PIN_BUZZER (and PIN_BUZZER_BRIDGE in antiphase), clocked from the crystal so DFS (80 <-> 40 MHz)
// does not bend the pitch. An esp_timer steps through the notes; light sleep
// is held off only while a sound plays.
//
// It only ever sounds in answer to input (and once at boot): scheduled night
// changes are silent, so it never beeps in a pocket on its own.
// Mute is a menu setting, kept in NVS by main.c (sound_set_muted at boot).

#pragma once

#include "config.h"
#include "input/input_event.h"

#include "app/app.h"

#if FEATURE_SOUND
void sound_init(void);
void sound_hello(void);
void sound_on_event(input_event_t ev);
void sound_fx(app_sound_t fx);  // menu and game sounds
void sound_set_muted(bool muted);
#else
static inline void sound_init(void) {}
static inline void sound_hello(void) {}
static inline void sound_on_event(input_event_t ev) { (void)ev; }
static inline void sound_fx(app_sound_t fx) { (void)fx; }
static inline void sound_set_muted(bool muted) { (void)muted; }
#endif
