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
        if (*s == '#' || *s == '_') {  // some collections write a sharp as _
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

// In the order Sasha set in the Yeti Running Order artifact
// (https://claude.ai/artifact/FzG9RZ1VHmfEmNqoHkGEmB, db `order/main`): the
// jukebox steps through them in this order. Korobeiniki, Jingle Bells and
// Happy Birthday are public domain, written two octaves up (most notes at
// 1.5-4 kHz), where a small passive buzzer with no transistor is loudest.
// The rest are from the RTTTL-Ringtones collection
// (huggingface.co/datasets/cosimoiaia/RTTTL-Ringtones), picked by Sasha,
// raised one or two octaves. Supergirl (Reamonn, 2000) is the chorus of the vocal line
// from Songsterr's transcription (songsterr.com, song 16994), three octaves up.
static const struct {
    const char *name, *rtttl;
} SONGS[] = {
    {"BARBIE GIRL",
     "barbiegirl:d=4,o=6,b=125:8g#,8e,8g#,8c#7,a,p,8f#,8d#,8f#,8b,g#,8f#,8e,p,8e,8c#,f#,c#,p,"
     "8f#,8e,g#,f#"},
    {"FINAL COUNTDOWN",
     "finalcou:d=4,o=6,b=140:16c#7,32b,32p,8c#.7,16p,f#,p.,32d7,32p,32c#7,32p,32d7,16p.,32c#7,"
     "16p.,b.,p,32d7,32p,32c#7,32p,d7,f#,p.,32b,32p,32a,32p,32b,16p.,32a,16p.,32g#,16p.,32b,"
     "16p.,a.,32c#7,32p,32b,32p,c#7,2f#,p,16p,32d7,32p,32c#7,32p,32d7,16p.,32c#7,16p.,b.,p,"
     "32d7,32p,32c#7,32p,d7,f#,p.,32b,32p,32a,32p,32b,16p.,32a,16p.,32g#,16p.,32b,16p.,2a,16p,"
     "32g#,32p,32a,32p,b.,16a,16b,8c#7,8b,8a,8g#,f#,d7,1c#7,8p,16c#7,16d7,16c#7,16b,2c#.7,16p"},
    {"TAKE ON ME",
     "takeonme:d=4,o=6,b=160:8f#7,8f#7,8f#7,8d7,8p,8b,8p,8e7,8p,8e7,8p,8e7,8g#7,8g#7,8a7,8b7,"
     "8a7,8a7,8a7,8e7,8p,8d7,8p,8f#7,8p,8f#7,8p,8f#7,8e7,8e7,8f#7,8e7,8f#7,8f#7,8f#7,8d7,8p,"
     "8b,8p,8e7,8p,8e7,8p,8e7,8g#7,8g#7,8a7,8b7,8a7,8a7,8a7,8e7,8p,8d7,8p,8f#7,8p,8f#7,8p,"
     "8f#7,8e7,8e7,8f#7,8e7"},
    {"MORTAL KOMBAT",
     "mortalko:d=4,o=7,b=35:32a#6,32a#6,32c#7,32a#6,32d#7,32a#6,32f7,32d#7,32c#7,32c#7,32f7,"
     "32c#7,32g#7,32c#7,32f7,32c#7,32g#6,32g#6,32c7,32g#6,32c#7,32g#6,32d#7,32c#7,32f#6,32f#6,"
     "32a#6,32f#6,32c#7,32f#6,32c#7,32c7,32a#6,32a#6,32c#7,32a#6,32d#7,32a#6,32f7,32d#7,32c#7,"
     "32c#7,32f7,32c#7,32g#7,32c#7,32f7,32c#7,32g#6,32g#6,32c7,32g#6,32c#7,32g#6,32d#7,32c#7,"
     "32f#6,32f#6,32a#6,32f#6,32c#7,32f#6,32c#7,32c7"},
    {"KOROBEINIKI",
     "Korobeiniki:d=4,o=7,b=160:e,8b6,8c,d,8c,8b6,a6,8a6,8c,e,8d,8c,b.6,8c,d,e,c,a6,2a6,8p,d,8f,"
     "a,8g,8f,e.,8c,e,8d,8c,b6,8b6,8c,d,e,c,a6,a6,p"},
    {"HARRY POTTER",
     "harrypot:d=4,o=6,b=200:e.,8g,f#,2e,b,2a.,2f#.,e.,8g,f#,2d,f,2c.,p,c,e.,8g,f#,2e,b,2d7,"
     "c#7,2c7,g#,2f7,e7,2d#7,b,2c.,p,e.,8g,f#,2e,b,2a.,2f#.,e.,8g,f#,2d,f,2c.,p,c,e.,8g,f#,2e,"
     "b,2d7,c#7,2c7,g#,2f7,e7,2d#7,b,2c."},
    {"IMPERIAL MARCH",
     "imperial:d=4,o=6,b=112:8g,16p,8g,16p,8g,16p,16d#.,32p,32a#.,8g,16p,16d#.,32p,32a#.,g,8p,"
     "32p,8d7,16p,8d7,16p,8d7,16p,16d#.7,32p,32a#.,8f#,16p,16d#.,32p,32a#.,g,8p,32p,8g7,16p,"
     "16g.,32p,32g.,8g7,16p,16f#.7,32p,32f.7,32e.7,32d#.7,16e7,8p,16g#,32p,8c#7,16p,16c.7,32p,"
     "32b.,32a#.,32a.,16a#,8p,16d#,32p,8f#,16p,16d#.,32p,32g.,8a#,16p,16g.,32p,32a#.,d7,8p,"
     "32p,8g7,16p,16g.,32p,32g.,8g7,16p,16f#.7,32p,32f.7,32e.7,32d#.7,16e7,8p,16g#,32p,8c#7,"
     "16p,16c.7,32p,32b.,32a#.,32a.,16a#,8p,16d#,32p,8f#,16p,16d#.,32p,32g.,8g,16p,16d#.,32p,"
     "32a#.,g"},
    {"JINGLE BELLS",
     "Jingle:d=8,o=7,b=125:e,e,4e,e,e,4e,e,g,c.,16d,2e,f,f,f.,16f,f,e,e,16e,16e,e,d,d,e,4d,4g,"
     "e,e,4e,e,e,4e,e,g,c.,16d,2e,f,f,f.,16f,f,e,e,16e,16e,g,g,f,d,2c"},
    {"LAST CHRISTMAS",
     "lastchri:d=4,o=6,b=112:16d7,e7,8p,e7,8d7,8p,8a,8e7,8e7,8f#7,d.7,8b,8b,8e7,8e7,f#7,d.7,"
     "8b,8c#7,8d7,8c#7,2b.,16e7,f#7,8p,e.7,8p,8b,8f#7,8g7,8f#7,2e7,8d7,8c#7,8d7,8c#7,c#7,8d7,"
     "8p,8c#7,8p,2a,16d7,e7,8p,e7,8d7,8p,8a,8e7,8e7,8f#7,d.7,8b,8b,8e7,8e7,f#7,d.7,8b,8c#7,"
     "8d7,8c#7,2b.,16e7,f#7,8p,e.7,8p,8b,8f#7,8g7,8f#7,2e7,8d7,8c#7,8d7,8c#7,c#7,8d7,8p,8c#7,"
     "8p,a"},
    {"HAPPY BIRTHDAY",
     "Birthday:d=4,o=7,b=120:8g.6,16g6,a6,g6,c,2b6,8g.6,16g6,a6,g6,d,2c,8g.6,16g6,g,e,c,b6,a6,"
     "8f.,16f,e,c,d,2c"},
    {"SUPERGIRL",
     "supergirl:d=8,o=6,b=116:16a,16p,b.,16p,c#7,4c#.7,p,16a,16p,b,16b,16p,c#7,4c#7,4p,a,4b,"
     "g#,4g#,4p,16b,16p,b,16b,16p,b,b,a,b,a,c#7,f#,16f#,16p,f#,2f#,4p.,f#,c#7,4b,a.,32a,32p,"
     "a.,16p,a,2a,2p,16a,16p,b.,16p,c#7,4c#.7,p,a,b,16b,16p,c#7,4c#7,4p,a,4b,g#,4g#,4p,16b,"
     "16p,b,16b,16p,b,b,a,b,a,c#7,f#,16f#,16p,f#,2f#,4p.,f#,c#7,4b,a.,32a,32p,a.,16p,a,2a"},
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
