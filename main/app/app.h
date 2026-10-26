// The menu and the games: everything that takes over the screen from the face.
//
// Pure C, no ESP-IDF (host-tested, like the face). The hardware comes in
// through app_env_t: sounds, saving the best scores, muting, tilt. Time comes
// in as a millisecond counter; randomness from a seeded generator.
//
// Controls, the same on every screen:
//   face:  B long opens the menu (main.c)
//   menu:  B next item, A choose, B long back to the face
//   game:  A plays (each game says how), B back to the menu, B long to the face
//
// The menu has two lists: the top one (feed, games, stats, sleep, sound,
// setup, back) and the games list it opens.
// The menu closes itself after APP_MENU_IDLE_MS, a game after APP_GAME_IDLE_MS
// without input, so a static screen never sits there burning in.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "display/gfx.h"
#include "face/rng.h"
#include "input/input_event.h"
#include "pet/pet.h"

#define APP_MENU_IDLE_MS 15000
#define APP_GAME_IDLE_MS 30000

typedef enum {
    SND_MOVE,    // menu: next item
    SND_SELECT,  // menu: open
    SND_BACK,
    SND_ROLL,    // dice rattling
    SND_DING,    // dice landed
    SND_COIN,    // coin landed
    SND_GO,      // reaction: now!
    SND_FAIL,    // too soon, hit, crash
    SND_POINT,
    SND_FLAP,
    SND_OVER,
    SND_RECORD,  // new best score
    SND_EAT,     // chomp
    SND_LEVELUP,
    SND_TONE_L,  // memory: left eye
    SND_TONE_R,  // memory: right eye
    SND_HI,      // picked up
    SND_SCARED,  // upside down
    SND_WHEE,    // falling / thrown
    SND_SNEEZE,
    SND_HUM,
    SND_COUNT
} app_sound_t;

// What survives power-off. main.c keeps it in NVS.
typedef struct {
    uint16_t reaction_ms;  // best reaction time, 0 = none yet
    uint16_t catch_pts;    // best scores
    uint16_t flappy_pts;
    uint16_t snake_pts;
    uint16_t memory_pts;
    uint8_t muted;
    int8_t orient[3];  // accelerometer calibration (input/orient.h), 0 = none
    pet_t pet;
} app_save_t;

typedef struct {
    void (*play)(app_sound_t s);
    void (*save)(const app_save_t *s);  // a best score, a setting or the pet changed
    void (*mute)(bool muted);
    bool (*tilt)(int *mg);  // left-right tilt in mg, + = right; false = no sensor
    bool (*accel)(int mg[3]);              // raw board-axis sample, for calibration
    void (*orient)(const int8_t map[3]);  // apply a new calibration
} app_env_t;

typedef enum {
    // Top list
    ITEM_FEED,
    ITEM_GAMES,
    ITEM_STATS,
    ITEM_SLEEP,
    ITEM_SOUND,
    ITEM_SETUP,
    ITEM_BACK,
    // Games list
    ITEM_DICE,
    ITEM_COIN,
    ITEM_REACTION,
    ITEM_CATCH,
    ITEM_FLAPPY,
    ITEM_SNAKE,
    ITEM_MEMORY,
    ITEM_UP,  // back to the top list
    ITEM_COUNT
} app_item_t;

typedef enum { APP_OFF = 0, APP_MENU, APP_GAME } app_mode_t;

// --- per-game state (only the running game's is meaningful) ---

typedef struct {
    enum { DICE_IDLE, DICE_ROLLING, DICE_SHOWN } phase;
    uint32_t t0;
    int value;  // 1..6, the result
    int shown;  // the face drawn right now (flips while rolling)
    uint32_t next_flip;
} dice_t;

typedef struct {
    enum { COIN_IDLE, COIN_FLIP, COIN_SHOWN } phase;
    uint32_t t0;
    bool heads;
} coin_t;

typedef struct {
    enum { RX_READY, RX_WAIT, RX_GO, RX_RESULT, RX_EARLY, RX_SLOW } phase;
    uint32_t t0;     // when the phase began
    uint32_t go_at;  // RX_WAIT: when NOW! appears
    uint16_t ms;     // RX_RESULT: the time
    bool best;       // RX_RESULT: a new record
} reaction_t;

#define CATCH_MAX 6

typedef struct {
    enum { CATCH_START, CATCH_PLAY, CATCH_OVER } phase;
    int x;  // player centre, px
    int lives, score;
    struct {
        bool on, rock;
        int x;
        int y;  // 1/16 px
    } obj[CATCH_MAX];
    uint32_t next_spawn, last, t0;
    bool best;
} catch_t;

#define FLAPPY_PIPES 3

typedef struct {
    enum { FLAPPY_START, FLAPPY_PLAY, FLAPPY_OVER } phase;
    int y, vy;  // bird, 1/16 px and 1/16 px per tick
    struct {
        int x;    // left edge, 1/16 px
        int gap;  // centre of the gap, px
        bool passed;
    } pipe[FLAPPY_PIPES];
    int score;
    uint32_t last, acc, t0;
    bool best;
} flappy_t;

#define SNAKE_COLS 30
#define SNAKE_ROWS 14
#define SNAKE_MAX (SNAKE_COLS * SNAKE_ROWS)

typedef struct {
    enum { SNAKE_START, SNAKE_PLAY, SNAKE_OVER } phase;
    uint8_t body[SNAKE_MAX][2];  // ring buffer of cells, head at `head`
    int head, len;
    int dir;          // 0 right, 1 down, 2 left, 3 up
    int turns[2];     // queued turns (+1 right, -1 left), so fast presses count
    int nturns;
    int food[2];
    int score;
    uint32_t next_step, t0;
    bool best;
    bool swallow_b;  // B went down as a turn: its SHORT is not "back"
} snake_t;

#define MEMORY_MAX 32

typedef struct {
    enum { MEM_START, MEM_SHOW, MEM_INPUT, MEM_GOOD, MEM_OVER } phase;
    uint8_t seq[MEMORY_MAX];  // 0 = left (A), 1 = right (B)
    int len;   // current round's length
    int pos;   // showing / expecting this step
    int lit;   // eye lit right now: -1 none, 0 left, 1 right
    uint32_t t0, lit_until;
    int score;
    bool best;
    bool swallow_b;
} memory_t;

// Care screens: feeding, the stats page, calibration.
#define FOOD_KINDS 3  // plus a BACK choice

typedef struct {
    enum { FEED_PICK, FEED_EAT, FEED_YUM, FEED_FULL } phase;
    int choice;  // 0..FOOD_KINDS-1, FOOD_KINDS = back
    int bites;   // FEED_EAT: bites taken so far
    uint32_t t0;
} feed_t;

typedef struct {
    enum { SETUP_FLAT, SETUP_UPRIGHT, SETUP_DONE, SETUP_FAIL } phase;
    int flat[3];
    uint32_t t0;
} setup_t;

typedef struct {
    app_mode_t mode;
    app_item_t item;  // selected in the menu / running
    int list;         // menu list showing: 0 top, 1 games
    const app_env_t *env;
    app_save_t save;
    rng_t rng;
    bool has_tilt;  // Catch is only offered with an accelerometer

    uint32_t now;
    uint32_t last_input;
    app_item_t slide_from;  // menu slide animation
    uint32_t slide_t0;
    bool sliding;

    int mood;  // expression for the face when the app closes, -1 = none
    bool want_sleep;  // SLEEP was chosen: main puts the face to bed

    uint32_t pet_acc;     // ms toward the next pet minute
    uint32_t pet_last;    // last app_tick() time
    int pet_unsaved;      // pet minutes since the last save
    uint32_t levelup_t0;  // "LEVEL UP!" banner, while recent
    bool levelup;

    union {
        dice_t dice;
        coin_t coin;
        reaction_t rx;
        catch_t ctch;
        flappy_t fl;
        snake_t snake;
        memory_t mem;
        feed_t feed;
        setup_t setup;
    } g;
} app_t;

void app_init(app_t *a, const app_env_t *env, const app_save_t *save, uint32_t seed);

bool app_active(const app_t *a);
void app_open(app_t *a, uint32_t now);   // show the menu
void app_close(app_t *a);                // back to the face

void app_on_event(app_t *a, input_event_t ev, uint32_t now);
void app_update(app_t *a, uint32_t now);
void app_render(const app_t *a, gfx_t *g);
bool app_is_animating(const app_t *a);

// The face's reaction to how the app went (EXPR_*), once; -1 when none.
int app_take_mood(app_t *a);
// SLEEP was chosen in the menu (once).
bool app_take_sleep(app_t *a);

// Every frame, menu open or not: the pet's clock. Saves now and then.
void app_tick(app_t *a, uint32_t now, bool asleep);
// Input that reached the face (petting, poking, shaking), for the pet.
void app_face_event(app_t *a, input_event_t ev);
uint8_t app_needs(const app_t *a);  // PET_NEED_* bits

const char *app_item_name(app_item_t it);
