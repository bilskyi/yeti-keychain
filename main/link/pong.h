// Pong across two screens: BIT's and YETI-7's.
//
// The same file in both projects - BIT/firmware/main and
// KEYCHAIN/main/link - byte for byte, so the two halves of one game can
// never disagree about the rules.
//
// Each screen is its own half of the court, seen from its own side: my
// paddle along the bottom, the net at the top. The ball leaves through the
// top of mine and drops in through the top of theirs - the two players face
// each other, so left and right swap on the way over (pong_ball_in does it).
// Whoever has the ball on their screen owns it; the other only waits.
//
// Pure: no ESP-IDF header, no float, no allocation, a millisecond clock
// passed in. What must be said to the other side comes out of
// pong_take_out(); what the other side said goes in through pong_ball_in(),
// pong_they_missed(), pong_they_quit() and pong_heard().

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PONG_W 128
#define PONG_H 64
#define PONG_WIN 5                // points to win
#define PONG_PADDLE_W 26
#define PONG_PADDLE_Y 59          // the paddle's top row; it is 3 rows tall
#define PONG_BALL_R 2             // the ball is 2r+1 square
#define PONG_SPEED0 (52 * 256)    // px/s, Q8, at the serve
#define PONG_SPEED_MAX (130 * 256)
#define PONG_SPEEDUP 9            // percent faster at each return
#define PONG_PADDLE_SPEED (260 * 256)  // px/s, Q8: how fast the paddle chases its target
#define PONG_SERVE_MS 1500        // the ball sits on the paddle, then goes by itself
#define PONG_POINT_MS 1200        // the point is shown before the serve
#define PONG_LOST_MS 7000         // nothing heard from the other side this long: over
#define PONG_STEP_MAX_MS 60       // a late frame is not allowed to tunnel the ball

typedef enum {
    PONG_SERVE = 0,  // the ball rests on my paddle
    PONG_MINE,       // the ball is on my screen
    PONG_AWAY,       // on theirs
    PONG_OVER,
} pong_phase_t;

typedef enum {
    PONG_END_NONE = 0,
    PONG_END_WON,
    PONG_END_LOST,
    PONG_END_THEY_QUIT,
    PONG_END_GONE,  // nothing heard for PONG_LOST_MS
} pong_end_t;

typedef enum { PONG_OUT_NONE = 0, PONG_OUT_BALL, PONG_OUT_MISS } pong_out_kind_t;

// Things that happened in the last tick, for sounds and faces. A bitmask.
enum {
    PONG_EV_HIT = 1u << 0,     // my paddle returned it
    PONG_EV_WALL = 1u << 1,
    PONG_EV_MISSED = 1u << 2,  // I missed: a point to them
    PONG_EV_SCORED = 1u << 3,  // they missed: a point to me
    PONG_EV_OVER = 1u << 4,
    PONG_EV_ARRIVED = 1u << 5,  // the ball came over to my side
};

typedef struct {
    uint8_t kind;  // pong_out_kind_t
    int32_t x, vx, vy;  // BALL: where and how it left, in my coordinates
    uint8_t yours, mine;  // MISS: the score, from the receiver's side
} pong_out_t;

typedef struct {
    uint8_t phase;  // pong_phase_t
    uint8_t end;    // pong_end_t, once over
    uint8_t me, them;
    int32_t x, y;    // the ball's centre, Q8 px; y grows downward
    int32_t vx, vy;  // Q8 px per second
    int32_t speed;   // Q8 px per second, along the path
    int32_t paddle, target;  // the paddle's centre, Q8 px
    int32_t away_x;  // where the ball left, Q8 px, for the marker at the top
    uint32_t phase_at, last_ms, heard_at;
    uint32_t rng;
    uint8_t events;  // PONG_EV_* since the last tick handed them over
    pong_out_t out;  // waiting for pong_take_out()
    bool out_ready;
} pong_t;

// A fresh game, 0 : 0. `serve`: the ball starts on my paddle; otherwise it
// starts on theirs and I wait.
void pong_start(pong_t *p, bool serve, uint32_t seed, uint32_t now_ms);

// Where the paddle should go, centre, in px; clamped to the court. The paddle
// moves toward it at PONG_PADDLE_SPEED.
void pong_aim(pong_t *p, int x_px);

// Serve now rather than waiting out PONG_SERVE_MS. Only while serving.
void pong_launch(pong_t *p, uint32_t now_ms);

// One frame. Returns the PONG_EV_* that happened since the last one, the
// other side's news included.
uint8_t pong_tick(pong_t *p, uint32_t now_ms);

// What to tell the other side, if anything. One at a time; taken, it is gone.
bool pong_take_out(pong_t *p, pong_out_t *o);

// From the other side. x, vx, vy are as their pong_out_t had them, in their
// coordinates. Ignored unless the ball is away.
void pong_ball_in(pong_t *p, int32_t x, int32_t vx, int32_t vy, uint32_t now_ms);
// They missed: a point to me. The score comes as they count it.
void pong_they_missed(pong_t *p, uint8_t mine, uint8_t theirs, uint32_t now_ms);
void pong_they_quit(pong_t *p, uint32_t now_ms);
// Anything at all came from the other side, or they acknowledged anything.
void pong_heard(pong_t *p, uint32_t now_ms);

static inline bool pong_over(const pong_t *p) { return p->phase == PONG_OVER; }
static inline int pong_ball_px_x(const pong_t *p) { return (int)(p->x >> 8); }
static inline int pong_ball_px_y(const pong_t *p) { return (int)(p->y >> 8); }
static inline int pong_paddle_px(const pong_t *p) { return (int)(p->paddle >> 8); }
