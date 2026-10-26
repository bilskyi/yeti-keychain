// Shared between app.c (menu) and the game files. Not for the rest of the
// firmware: main.c only uses app.h.

#pragma once

#include "app/app.h"
#include "display/font.h"
#include "face/face.h"

// Each game: start fresh, take an event (false = the game did not use it),
// advance, draw, and say whether it is moving (fast frames).
#define GAME_FNS(name)                                              \
    void name##_start(app_t *a, uint32_t now);                     \
    bool name##_event(app_t *a, input_event_t ev, uint32_t now);   \
    void name##_update(app_t *a, uint32_t now);                    \
    void name##_render(const app_t *a, gfx_t *g);                  \
    bool name##_animating(const app_t *a)

GAME_FNS(dice);
GAME_FNS(coin);
GAME_FNS(reaction);
GAME_FNS(catch);
GAME_FNS(flappy);
GAME_FNS(snake);
GAME_FNS(memory);
GAME_FNS(feed);
GAME_FNS(stats);
GAME_FNS(setup);
GAME_FNS(music);
void music_leave(app_t *a);  // stops the song

// Helpers for the games.
void app_play(const app_t *a, app_sound_t s);
// Records a score if it beats the saved one (lower_is_better for times).
// Returns true for a new record, and saves it.
bool app_record(app_t *a, uint16_t *best, uint16_t value, bool lower_is_better);
// A round of a game finished: the pet had `fun`, spent `energy`, earned `xp`.
// Plays the level-up fanfare and shows the banner when it grew.
void app_reward(app_t *a, int fun, int energy, int xp);
// Shared end-of-game screen: GAME OVER, the score, NEW BEST or the best.
void app_game_over(gfx_t *g, int score, bool best, int saved_best);
// The bottom line: what A does on the left, what B does on the right.
void app_hint(gfx_t *g, const char *a_does, const char *b_does);
// A bar 0..PET_MAX at (x, y), w x h, outlined.
void app_bar(gfx_t *g, int x, int y, int w, int h, int value, int max);
// Sine of a 0..63 angle (one turn), x256.
int app_sin64(int a);
// Number to text, no libc printf needed.
void app_itoa(int v, char *out);
// The EMO face, small, for icons and game sprites.
void app_mini_face(gfx_t *g, int cx, int cy, int spread, int r, gfx_color_t c);
void app_heart(gfx_t *g, int cx, int cy, int s);

// since + ms has been reached by now. Signed difference, so it is wrap-safe
// and a moment still in the future (ms = 0, since > now) is not "elapsed".
static inline bool app_elapsed(uint32_t now, uint32_t since, uint32_t ms) {
    return (int32_t)(now - (since + ms)) >= 0;
}
