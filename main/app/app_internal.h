// Shared between app.c (menu) and the game files. Not for the rest of the
// firmware: main.c only uses app.h.

#pragma once

#include <stddef.h>

#include "app/app.h"
#include "display/font.h"
#include "face/face.h"

// A line in the device's log, for what is worth knowing after the fact (why a
// match ended). Nothing on the host, where there is no log to keep.
#ifdef ESP_PLATFORM
#include "esp_log.h"
#define APP_LOG(...) ESP_LOGI("app", __VA_ARGS__)
#else
#define APP_LOG(...) ((void)0)
#endif

// Each game: start fresh, take an event (false = the game did not use it),
// advance, draw, and say whether it is moving (fast frames).
#define GAME_FNS(name)                                              \
    void name##_start(app_t *a, uint32_t now);                     \
    bool name##_event(app_t *a, input_event_t ev, uint32_t now);   \
    void name##_update(app_t *a, uint32_t now);                    \
    void name##_render(const app_t *a, gfx_t *g);                  \
    bool name##_animating(const app_t *a)

GAME_FNS(dice);
GAME_FNS(reaction);
GAME_FNS(catch);
GAME_FNS(maze);
GAME_FNS(flappy);
GAME_FNS(snake);
GAME_FNS(voice);
GAME_FNS(setup);
GAME_FNS(update);
void update_leave(app_t *a);  // the WiFi down, if it was up
GAME_FNS(toss);
void toss_link(app_t *a, const bl_msg_t *m, uint32_t now);
void toss_link_done(app_t *a, const bl_msg_t *m, bool delivered, uint32_t now);
GAME_FNS(pongk);
void pongk_link(app_t *a, const bl_msg_t *m, uint32_t now);
void pongk_link_done(app_t *a, const bl_msg_t *m, bool delivered, uint32_t now);
void pongk_leave(app_t *a);  // says goodbye to BIT if a match or a question is under way
GAME_FNS(music);
void music_leave(app_t *a);  // stops the song
GAME_FNS(egg);

// Helpers for the games.
void app_play(const app_t *a, app_sound_t s);
// Records a score if it beats the saved one (lower_is_better for times).
// Returns true for a new record, and saves it; the face cheers afterwards.
bool app_record(app_t *a, uint16_t *best, uint16_t value, bool lower_is_better);
// Shared end-of-game screen: GAME OVER, the score, NEW BEST or the best.
void app_game_over(gfx_t *g, int score, bool best, int saved_best);
// The bottom line: what A, B and C do, each word above its button (left,
// middle, right). NULL leaves a button's place empty.
void app_hint(gfx_t *g, const char *a_does, const char *b_does, const char *c_does);
// A hint line for the title screen of a game: A starts, C goes back.
#define app_hint_start(g) app_hint(g, "START", NULL, "BACK")
// Sine of a 0..63 angle (one turn), x256.
int app_sin64(int a);
// Number to text, no libc printf needed.
void app_itoa(int v, char *out);
// The EMO face, small, for icons and game sprites.
void app_mini_face(gfx_t *g, int cx, int cy, int spread, int r, gfx_color_t c);
void app_heart(gfx_t *g, int cx, int cy, int s);
// Dino Roar's dinosaur, 16 x 15 (times scale) from its top left; pose 0 / 1
// running, 2 in the air. In art.c, with the pictures.
void app_dino(gfx_t *g, int x, int y, int pose, bool roaring, int scale);
// Sound waves from a mouth at x, y: n arcs.
void app_roar(gfx_t *g, int x, int y, int n);
// BIT's link, through the env: false / APP_LINK_OFF when there is no radio.
bool app_link_send(const app_t *a, const bl_msg_t *m, bool reliable);
app_link_t app_link_status(const app_t *a);
// A thrown thing, centred on (cx, cy): a heart, a star or a bomb, s px across.
void app_toss_item(gfx_t *g, int item, int cx, int cy, int s);

// since + ms has been reached by now. Signed difference, so it is wrap-safe
// and a moment still in the future (ms = 0, since > now) is not "elapsed".
static inline bool app_elapsed(uint32_t now, uint32_t since, uint32_t ms) {
    return (int32_t)(now - (since + ms)) >= 0;
}
