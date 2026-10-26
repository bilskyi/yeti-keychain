#include "sound/rtttl.h"

#include <stdbool.h>

// Octave 8, C..B, in Hz; lower octaves halve.
static const uint16_t OCT8[12] = {4186, 4435, 4699, 4978, 5274, 5588,
                                  5920, 6272, 6645, 7040, 7459, 7902};
// Semitone of each letter a..g from C.
static const int8_t SEMI[7] = {9, 11, 0, 2, 4, 5, 7};

static bool digit(char c) {
    return c >= '0' && c <= '9';
}

static int number(const char **p) {
    int v = 0;
    while (digit(**p)) v = v * 10 + (*(*p)++ - '0');
    return v;
}

static int lower(char c) {
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

int rtttl_parse(const char *s, note_t *out, int max, int gap_ms) {
    if (max < 1) return 0;
    out[0] = (note_t){0, 0};
    int dur = 4, oct = 6, bpm = 63;
    // Skip the name.
    while (*s && *s != ':') s++;
    if (*s != ':') return 0;
    s++;
    // Defaults.
    while (*s && *s != ':') {
        const char key = (char)lower(*s);
        if (key == 'd' || key == 'o' || key == 'b') {
            s++;
            if (*s == '=') s++;
            const int v = number(&s);
            if (key == 'd' && v) dur = v;
            if (key == 'o' && v) oct = v;
            if (key == 'b' && v) bpm = v;
        } else {
            s++;
        }
    }
    if (*s != ':') return 0;
    s++;
    const int whole_ms = 60000 * 4 / bpm;
    int n = 0;
    while (*s) {
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        int d = digit(*s) ? number(&s) : dur;
        if (d <= 0) d = dur;
        const int letter = lower(*s);
        if (!*s) break;
        s++;
        int semi = -1;  // rest
        if (letter >= 'a' && letter <= 'g') semi = SEMI[letter - 'a'];
        if (*s == '#') {
            semi++;
            s++;
        }
        bool dotted = false;
        if (*s == '.') {
            dotted = true;
            s++;
        }
        int o = oct;
        if (digit(*s)) o = *s++ - '0';
        if (*s == '.') {
            dotted = true;
            s++;
        }
        while (*s && *s != ',') s++;  // ignore anything else in the token

        int ms = whole_ms / d;
        if (dotted) ms += ms / 2;
        uint16_t hz = 0;
        if (semi >= 0) {
            if (semi == 12) {  // b#: the next octave's c
                semi = 0;
                o++;
            }
            const int shift = 8 - o;
            hz = shift >= 0 && shift < 8 ? (uint16_t)(OCT8[semi] >> shift) : 0;
        }
        const int gap = hz && gap_ms > 0 && ms > 2 * gap_ms ? gap_ms : 0;
        if (n + (gap ? 2 : 1) >= max) break;  // keep room for the terminator
        out[n++] = (note_t){hz, (uint16_t)(ms - gap)};
        if (gap) out[n++] = (note_t){0, (uint16_t)gap};
    }
    out[n] = (note_t){0, 0};
    return n;
}

// Written two octaves up (most notes at 1.5-4 kHz), where a small passive
// buzzer with no transistor is loudest.
static const struct {
    const char *name, *rtttl;
} SONGS[] = {
    {"JINGLE BELLS",
     "Jingle:d=8,o=7,b=125:e,e,4e,e,e,4e,e,g,c.,16d,2e,f,f,f.,16f,f,e,e,16e,16e,e,d,d,e,4d,4g,"
     "e,e,4e,e,e,4e,e,g,c.,16d,2e,f,f,f.,16f,f,e,e,16e,16e,g,g,f,d,2c"},
    {"KOROBEINIKI",
     "Korobeiniki:d=4,o=7,b=160:e,8b6,8c,d,8c,8b6,a6,8a6,8c,e,8d,8c,b.6,8c,d,e,c,a6,2a6,8p,d,8f,"
     "a,8g,8f,e.,8c,e,8d,8c,b6,8b6,8c,d,e,c,a6,a6,p"},
    {"MOUNTAIN KING",
     "Mountain:d=8,o=6,b=150:b,c#7,d7,e7,f#7,d7,4f#7,f7,c#7,4f7,e7,c7,4e7,b,c#7,d7,e7,f#7,d7,"
     "f#7,b7,a7,f#7,d7,f#7,2a7"},
    {"ODE TO JOY",
     "Ode:d=4,o=7,b=140:e,e,f,g,g,f,e,d,c,c,d,e,e.,8d,2d,e,e,f,g,g,f,e,d,c,c,d,e,d.,8c,2c"},
    {"TWINKLE",
     "Twinkle:d=4,o=7,b=160:c,c,g,g,a,a,2g,f,f,e,e,d,d,2c,g,g,f,f,e,e,2d,g,g,f,f,e,e,2d,c,c,g,g,"
     "a,a,2g,f,f,e,e,d,d,2c"},
    {"FUR ELISE",
     "Elise:d=16,o=7,b=125:e,d#,e,d#,e,b6,d,c,8a6,p,c6,e6,a6,8b6,p,e6,g#6,b6,8c,p,e6,e,d#,e,d#,e,"
     "b6,d,c,8a6,p,c6,e6,a6,8b6,p,e6,c,b6,4a6"},
    {"HAPPY BIRTHDAY",
     "Birthday:d=4,o=7,b=120:8g.6,16g6,a6,g6,c,2b6,8g.6,16g6,a6,g6,d,2c,8g.6,16g6,g,e,c,b6,a6,"
     "8f.,16f,e,c,d,2c"},
};

int songs_count(void) {
    return (int)(sizeof(SONGS) / sizeof(SONGS[0]));
}

const char *song_name(int i) {
    return i >= 0 && i < songs_count() ? SONGS[i].name : "?";
}

const char *song_rtttl(int i) {
    return i >= 0 && i < songs_count() ? SONGS[i].rtttl : "";
}
