// The menu and the games: everything that takes over the screen from the face.
//
// Pure C, no ESP-IDF (host-tested, like the face). The hardware comes in
// through app_env_t: sounds, saving, volume, brightness, tilt, the mic. Time
// comes in as a millisecond counter; randomness from a seeded generator.
//
// Three buttons, left to right A, B, C, and one rule for each on every screen:
//   A  do it: choose, change a setting, play (each game says how)
//   B  next: the next item, the next song; a second action in some games
//   C  back: one step toward the face. Held: straight home to the face.
// On the face (main.c): A pets it, B opens the games, C the status card with
// the settings behind it, and holding B opens the music. No categories to
// go through: each is one press from the face.
// The bottom line of every screen says what each button does, each word
// above its own button.
//
// The games list opens on the last game played. A list closes itself after
// APP_MENU_IDLE_MS, a game after APP_GAME_IDLE_MS without input, so a static
// screen never burns in.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "display/gfx.h"
#include "face/rng.h"
#include "app/music_viz.h"
#include "input/input_event.h"
#include "link/bitlink.h"
#include "link/pong.h"

#define APP_MENU_IDLE_MS 15000
#define APP_GAME_IDLE_MS 30000

#define APP_VOLUME_LEVELS 4      // off, low, medium, high
#define APP_BRIGHTNESS_LEVELS 3  // low, medium, high

typedef enum {
    SND_MOVE,    // menu: next item
    SND_SELECT,  // menu: open
    SND_BACK,
    SND_ROLL,    // dice rattling
    SND_DING,    // dice landed
    SND_GO,      // reaction: now!
    SND_FAIL,    // too soon, hit, crash
    SND_POINT,
    SND_FLAP,
    SND_OVER,
    SND_RECORD,  // new best score
    SND_HI,      // picked up
    SND_SCARED,  // upside down
    SND_WHEE,    // falling / thrown
    SND_SNEEZE,
    SND_HUM,
    SND_GLITCH,  // garbled chirps
    SND_HMPH,    // offended (shaken quiet mid-song)
    SND_PURR,    // petted (repeats while the touch is held)
    SND_GROWL,   // shaken twice: getting cross
    SND_FURY,    // shaken on and on: furious
    SND_KO,      // bonk, and down it goes
    SND_AWW,     // forgiven
    SND_SIGH,    // melting under the petting
    SND_ZAP,     // shaken while hung: a jolt
    SND_ERROR,   // anything else while hung
    SND_POWERDOWN,  // shaken free: powering down...
    SND_BOOT,       // ...and back up
    SND_HEARTBEAT,  // petting's heart overload: ba-dum
    SND_SPARKLE,    // petting's finale
    SND_BUZZ,       // a fly
    SND_TWINKLE,    // snow
    SND_ROFL,       // rolling with laughter
    SND_STRETCH,    // waking up: mmmm-ah
    SND_JUMP,       // voice game: a hop
    SND_CHARGE,     // charger plugged in: powering up
    SND_UNPLUG,     // charger pulled out
    SND_EGG_RISE,   // the 67 easter egg: charging up
    SND_EGG_SIX,    // ...the 6 slams in
    SND_EGG_SEVEN,  // ...the 7 slams in
    SND_EGG_TICK,   // ...each swing of the hands
    SND_EGG_BOOM,   // ...it blows up
    SND_COUNT
} app_sound_t;

// What survives power-off. main.c keeps it in NVS.
typedef struct {
    uint16_t reaction_ms;  // best reaction time, 0 = none yet
    uint16_t catch_pts;    // best scores
    uint16_t flappy_pts;
    uint16_t snake_pts;
    uint16_t voice_pts;
    uint16_t maze_pts;
    uint8_t volume;      // 0 = off .. APP_VOLUME_LEVELS - 1
    uint8_t brightness;  // 0 .. APP_BRIGHTNESS_LEVELS - 1
    int8_t orient[3];    // accelerometer calibration (input/orient.h), 0 = none
} app_save_t;

// A first boot: medium volume and brightness.
#define APP_SAVE_DEFAULTS {.volume = 2, .brightness = 1}

// An update over WiFi (net/update.h), as the UPDATE screen shows it.
typedef enum {
    APP_UPD_OFF = 0,
    APP_UPD_WAITING,    // the WiFi is up; nobody on it
    APP_UPD_JOINED,     // a phone or laptop joined it
    APP_UPD_RECEIVING,  // a file is coming in
    APP_UPD_DONE,       // written and verified: restarting into it
    APP_UPD_FAILED,     // `why` says what
} app_upd_phase_t;

typedef struct {
    uint8_t phase;  // an app_upd_phase_t
    uint8_t pct;    // APP_UPD_RECEIVING
    char ssid[16];
    char pass[12];
    char addr[16];
    const char *why;  // APP_UPD_FAILED, short, upper case
} app_update_t;

typedef struct {
    void (*play)(app_sound_t s);
    void (*save)(const app_save_t *s);  // a best score or a setting changed
    void (*volume)(int level);          // apply a new volume, 0 = off
    void (*brightness)(int level);      // apply a new brightness
    bool (*tilt)(int *mg);  // left-right tilt in mg, + = right; false = no sensor
    bool (*tilt_xy)(int mg[2]);  // downhill in the screen's plane, mg: [0] + = right, [1] + = down
    bool (*accel)(int mg[3]);              // raw board-axis sample, for calibration
    void (*orient)(const int8_t map[3]);  // apply a new calibration
    bool (*song)(int i);                  // play song i (sound/rtttl.h); false = sound off
    void (*song_stop)(void);
    bool (*song_playing)(void);
    int (*loudness)(void);        // the mic now, 0-100; -1 = no mic
    bool (*battery)(int *pct);    // false = no battery (on USB)
    int (*battery_mv)(void);      // the last reading, mV; -1 = none
    bool (*charging)(void);       // the charger's cable is in
    // BIT, over the radio (link/buddy.h). Any of these may be NULL: no radio.
    bool (*link_send)(const bl_msg_t *m, bool reliable);
    int (*link_status)(void);          // an app_link_t
    uint32_t (*link_heard_at)(void);   // when BIT was last heard, ms; 0 never
    // An update over WiFi (net/update.h). NULL: no such thing on this build.
    bool (*update_begin)(void);
    void (*update_end)(void);
    void (*update_status)(app_update_t *u);
} app_env_t;

// How things stand with BIT (link/buddy.h's buddy_status_t, in the app's words).
typedef enum { APP_LINK_OFF = 0, APP_LINK_SEARCHING, APP_LINK_AWAY, APP_LINK_NEAR } app_link_t;

typedef enum {
    // Games (B on the face)
    ITEM_DICE,
    ITEM_REACTION,
    ITEM_CATCH,
    ITEM_MAZE,
    ITEM_FLAPPY,
    ITEM_SNAKE,
    ITEM_VOICE,
    ITEM_BIT,   // throw BIT something
    ITEM_PONG,  // Pong against BIT, across both screens
    // Status and settings (C on the face)
    ITEM_STATUS,
    ITEM_VOLUME,
    ITEM_BRIGHTNESS,
    ITEM_SETUP,
    ITEM_UPDATE,  // a new firmware over WiFi
    // The jukebox (B held on the face)
    ITEM_MUSIC,
    // The easter egg: six taps on A, then a shake (app_egg_watch)
    ITEM_EGG,
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
} snake_t;

#define VOICE_ROCKS 3

typedef struct {
    enum { VOICE_START, VOICE_PLAY, VOICE_OVER } phase;
    int y, vy;  // the Yeti's feet above the ground, 1/16 px; up is +
    struct {
        int x;  // left edge, 1/16 px
        int h;  // height, px
        bool passed;
    } rock[VOICE_ROCKS];
    int score;
    int level;  // the mic's last loudness, 0-100, for the meter
    uint32_t last, acc, t0;
    bool best, no_mic;
} voice_t;

#define MAZE_COLS 15  // at the smallest cell size
#define MAZE_ROWS 7

typedef struct {
    enum { MAZE_START, MAZE_PLAY, MAZE_SOLVED, MAZE_OVER } phase;
    uint8_t walls[MAZE_ROWS][MAZE_COLS];  // per cell: right and bottom wall bits
    int cols, rows, cell, ox, oy;         // this maze's grid and where it is drawn
    int goal;                             // cell index, row * cols + col
    int x, y, vx, vy;  // the ball: 1/16 px, 1/16 px per second
    int base[2];       // the tilt when the run began: that counts as level
    int level, score;
    int left_ms;
    uint32_t last, t0;
    bool best;
} maze_t;

typedef struct {
    enum { TOSS_AIM, TOSS_FLYING, TOSS_WAIT, TOSS_RESULT } phase;
    int item;          // a bl_item_t
    uint32_t t0;       // the phase began
    uint8_t result;    // TOSS_RESULT: a bl_caught_t, or TOSS_MISSED / TOSS_NO_BIT
    bool delivered;    // BIT's radio took it
} toss_t;

#define TOSS_MISSED 0x80
#define TOSS_NO_BIT 0x81

typedef struct {
    enum { PK_TITLE, PK_ASKING, PK_NO, PK_PLAY, PK_OVER } phase;
    uint32_t t0;
    uint8_t why;      // PK_NO: a bl_no_t, or 0 BIT never heard, TOSS_NO_BIT none known
    uint8_t end;      // PK_OVER: a pong_end_t, or PK_I_QUIT
    pong_t pong;
    uint32_t next_alive;
    uint32_t point_at;  // the last point, for the score flash; 0 none
    bool point_mine;
} pongk_t;

#define PK_I_QUIT 0x10

typedef struct {
    uint32_t t0;   // opened, or the last sign of anyone using it
    bool started;  // the WiFi came up
} upd_t;

typedef struct {
    enum { SETUP_FLAT, SETUP_UPRIGHT, SETUP_DONE, SETUP_FAIL } phase;
    int flat[3];
    uint32_t t0;
} setup_t;

typedef struct {
    int song;
    bool playing;
    uint32_t t0;         // the song started
    uint32_t last;       // the last update: the animation's clock
    int next;            // the next entry of the track to start...
    uint32_t next_at;    // ...and when, ms into the song
    uint32_t msg_until;  // "SOUND OFF" shows until then
    viz_t viz;
    viz_track_t track;
} music_t;

#define EGG_MS 7200

typedef struct {
    uint32_t t0;
    uint32_t cued;  // the sounds played so far, a bit each
} egg_t;

typedef struct {
    app_mode_t mode;
    app_item_t item;  // selected in the menu / running
    int list;         // menu list showing: 0 games, 1 status and settings
    app_item_t last_game;  // the games list opens on it
    int last_song;         // the jukebox starts on it
    int last_viz;          // the last song's animation: the next one is different, -1 = none yet
    int egg_taps;          // A presses in a row on the face, toward the easter egg
    uint32_t egg_at;       // the last one
    const app_env_t *env;
    app_save_t save;
    rng_t rng;
    bool has_tilt;  // Catch, Maze and calibration need the accelerometer

    uint32_t now;
    uint32_t last_input;
    app_item_t slide_from;  // menu slide animation
    uint32_t slide_t0;
    int slide_dir;          // +1 to the next item, -1 back up a level
    bool sliding;

    int mood;  // expression for the face when the app closes, -1 = none

    union {
        dice_t dice;
        reaction_t rx;
        catch_t ctch;
        maze_t maze;
        flappy_t fl;
        snake_t snake;
        voice_t voice;
        setup_t setup;
        toss_t toss;
        pongk_t pong;
        upd_t upd;
        music_t music;
        egg_t egg;
    } g;
} app_t;

void app_init(app_t *a, const app_env_t *env, const app_save_t *save, uint32_t seed);

bool app_active(const app_t *a);
void app_open_games(app_t *a, uint32_t now);     // the games list, on the last game played
void app_open_settings(app_t *a, uint32_t now);  // the status card, settings after it
void app_open_music(app_t *a, uint32_t now);     // the jukebox, on the last song
// Every event on the face goes through here first: six quick presses of A,
// then a shake, open the 67 easter egg. True when it fired (the shake is
// used up); the presses still pet the face as usual.
bool app_egg_watch(app_t *a, input_event_t ev, uint32_t now);
void app_close(app_t *a);                // back to the face

void app_on_event(app_t *a, input_event_t ev, uint32_t now);
void app_update(app_t *a, uint32_t now);
void app_render(const app_t *a, gfx_t *g);
bool app_is_animating(const app_t *a);
bool app_wants_mic(const app_t *a);  // the voice game is listening
// A BIT screen is up: keep the radio listening for it (link/buddy.h).
bool app_wants_link(const app_t *a);
// A frame from BIT, and the fate of one sent to it reliably.
void app_on_link(app_t *a, const bl_msg_t *m, uint32_t now);
void app_on_link_done(app_t *a, const bl_msg_t *m, bool delivered, uint32_t now);

// The face's reaction to how the app went (EXPR_*), once; -1 when none.
int app_take_mood(app_t *a);

const char *app_item_name(app_item_t it);

// Display contrast for a brightness level (main.c applies it).
uint8_t app_contrast(int brightness);
