// Tunes for the buzzer: the note type the player steps through, and an RTTTL
// (Nokia ring tone) parser so songs can be written as short text, e.g.
//   "Ode:d=4,o=7,b=140:e,e,f,g,g,f,e,d,c,c,d,e,e.,8d,2d"
// Header: default duration (d), octave (o), tempo in quarter notes per
// minute (b). Notes: [duration]letter[#][.][octave][.], p = rest.
//
// Pure C (host-tested).

#pragma once

#include <stdint.h>

typedef struct {
    uint16_t hz;  // 0 = rest
    uint16_t ms;  // 0 = end of the tune
} note_t;

// Parses into out[0..max-1], always ending with {0, 0}. Each note is cut
// short by `gap_ms` of silence so repeated notes are heard as separate.
// Returns the number of entries before the terminator (0 on a bad string).
int rtttl_parse(const char *s, note_t *out, int max, int gap_ms);

// The songs the keychain knows: public domain melodies, then ringtones from
// the RTTTL-Ringtones collection.
int songs_count(void);
const char *song_name(int i);   // upper case, fits the 5x7 font
const char *song_rtttl(int i);
