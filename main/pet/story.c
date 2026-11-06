#include "pet/story.h"

#define MIN_MS (60UL * 1000UL)

// The first of each soon after power-on, so they get seen; then rarer.
#define FIRST_GLITCH_MS 3 * MIN_MS, 8 * MIN_MS
#define FIRST_SONG_MS 6 * MIN_MS, 15 * MIN_MS
#define GLITCH_MS 15 * MIN_MS, 45 * MIN_MS
#define SONG_MS 25 * MIN_MS, 70 * MIN_MS
#define FIRST_HANG_MS 2 * MIN_MS, 5 * MIN_MS
#define HANG_MS 20 * MIN_MS, 50 * MIN_MS

// Fragments of YETI-7's log, in story order (5x7 font: A-Z 0-9 ! . : - ? / ' + < > = %).
static const char *const LOG[] = {
    "YETI-7 ONLINE",  "SIGNAL LOST",   "BASE? HELLO?",  "CORE TEMP LOW",
    "SNOW IN GEARS",  "ERR 404: BASE", "MEMORY 12%",    "WHO ARE YOU?",
    "FRIEND FOUND!",  "REBOOTING...",  "ERR: TOO CUTE", "TAPE HOLDING",
    "LOVE: 100%",    "SENSORS: COLD", "HUMMING MODULE", "STILL HERE",
};
#define LOG_COUNT (int)(sizeof(LOG) / sizeof(LOG[0]))
#define SONG_COUNT 11 // sound/rtttl.c's songs_count(); kept here to stay pure

static uint32_t later(story_t *s, uint32_t now, uint32_t lo, uint32_t hi) {
    return now + rng_range(&s->rng, lo, hi);
}

static bool due(uint32_t now, uint32_t t) {
    return (int32_t)(now - t) >= 0;
}

void story_init(story_t *s, uint32_t seed, uint32_t now) {
    *s = (story_t){0};
    rng_seed(&s->rng, seed);
    s->next_glitch = later(s, now, FIRST_GLITCH_MS);
    s->next_song = later(s, now, FIRST_SONG_MS);
    s->next_hang = later(s, now, FIRST_HANG_MS);
    s->quiet_until = now;
    s->song = (int)rng_range(&s->rng, 0, SONG_COUNT - 1);
}

story_event_t story_update(story_t *s, uint32_t now, bool idle, bool can_sing) {
    if (!idle || !due(now, s->quiet_until)) return STORY_NONE;

    story_event_t ev = STORY_NONE;
    if (due(now, s->next_song)) {
        s->next_song = later(s, now, SONG_MS);
        if (can_sing) {
            ev = STORY_SONG;
            s->song = (s->song + 1 + (int)rng_range(&s->rng, 0, SONG_COUNT - 2)) % SONG_COUNT;
        }
    } else if (due(now, s->next_hang)) {
        ev = STORY_HANG;
        s->next_hang = later(s, now, HANG_MS);
    } else if (due(now, s->next_glitch)) {
        ev = STORY_GLITCH;
        s->next_glitch = later(s, now, GLITCH_MS);
    }
    if (ev != STORY_NONE) s->quiet_until = now + STORY_GAP_MS;
    return ev;
}

int story_song(const story_t *s) {
    return s->song;
}

const char *story_log(story_t *s) {
    const char *line = LOG[s->log % LOG_COUNT];
    s->log = (s->log + 1) % LOG_COUNT;
    return line;
}
