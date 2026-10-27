// Passive buzzer: short chirps that go with the face's reactions.
//
// LEDC PWM on PIN_BUZZER (and PIN_BUZZER_BRIDGE in antiphase), clocked from the crystal so DFS (80 <-> 40 MHz)
// does not bend the pitch. An esp_timer steps through the notes; light sleep
// is held off only while a sound plays.
//
// It sounds in answer to input, once at boot, and for the pet's own events
// (alarms when it needs something, glitches, a random song; story.c), never
// at night.
// Mute is a menu setting, kept in NVS by main.c (sound_set_muted at boot).

#pragma once

#include "config.h"
#include "input/input_event.h"

#include "app/app.h"

#if FEATURE_SOUND
void sound_init(void);
void sound_hello(void);
void sound_goodnight(void);
void sound_on_event(input_event_t ev);
void sound_fx(app_sound_t fx);  // menu and game sounds
void sound_set_muted(bool muted);
// Songs (sound/rtttl.h). A song is cut off by any other sound.
bool sound_song(int i);  // false when muted or no such song
void sound_stop(void);
bool sound_song_playing(void);
// Answering someone who talked: one chirp per syllable (1-9).
void sound_babble(int syllables);
bool sound_busy(void);  // anything playing: the mic is deaf meanwhile
// Called (from the timer task) as each note of a song or babble starts; for the LED.
void sound_set_note_hook(void (*hook)(uint16_t hz, uint16_t ms));
#else
static inline void sound_init(void) {}
static inline void sound_hello(void) {}
static inline void sound_goodnight(void) {}
static inline void sound_on_event(input_event_t ev) { (void)ev; }
static inline void sound_fx(app_sound_t fx) { (void)fx; }
static inline void sound_set_muted(bool muted) { (void)muted; }
static inline bool sound_song(int i) { (void)i; return false; }
static inline void sound_stop(void) {}
static inline bool sound_song_playing(void) { return false; }
static inline void sound_babble(int syllables) { (void)syllables; }
static inline bool sound_busy(void) { return false; }
static inline void sound_set_note_hook(void (*hook)(uint16_t hz, uint16_t ms)) { (void)hook; }
#endif
