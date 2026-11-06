// bitlink: what BIT and YETI-7 (the keychain) say to each other over ESP-NOW.
//
// The same file in both projects - BIT/firmware/main and
// KEYCHAIN/main/link - byte for byte. Change one, copy it to the other, and
// bump BL_VERSION if the frame changed: a frame of another version is
// dropped, not misread.
//
// Pure: no ESP-IDF header, no allocation, a millisecond clock passed in. The
// radio itself is each project's own glue (buddy.c); this is the wire format,
// the resending, the duplicate filter and the "is the other one near" clock,
// so all of it is tested on the laptop.
//
// Who says what. YETI is the one that moves, so it does the asking: it
// finds BIT (BIT says HELLO every second, on its router's channel), looks
// for it now and then while awake, throws things at it and invites it to
// Pong. BIT only ever answers. Its radio dozes between the router's beacons
// and hears nothing then, so it wakes for a moment after each HELLO - YETI
// answers into that moment - and stays awake while YETI is talking to it.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BL_VERSION 1
#define BL_FRAME_BYTES 28

typedef enum {
    BL_HELLO = 1,  // broadcast: here I am. name = who. From BIT also, for YETI's log:
                   // a, b = frames heard from YETI (to BIT, to all); d = 1 radio awake
    BL_PING,       // YETI -> BIT: still near. d = 1: a BIT screen is open, stay awake
    BL_THROW,      // YETI -> BIT: d = a bl_item_t, flying over
    BL_CAUGHT,     // BIT -> YETI: d = the item, e = a bl_caught_t
    BL_INVITE,     // YETI -> BIT: a game of Pong?
    BL_ANSWER,     // BIT -> YETI: d = 1 yes, 0 no; e = a bl_no_t when no
    BL_BALL,       // Pong: the ball leaves my screen at the top. a = x, b = vx, c = vy (pong.h units)
    BL_MISS,       // Pong: I missed it. d = your score, e = mine
    BL_QUIT,       // Pong: I have left the game
    BL_ALIVE,      // Pong: still in the game (sent unreliably, twice a second)
    BL_TYPE_COUNT
} bl_type_t;

typedef enum { BL_ROLE_BIT = 1, BL_ROLE_YETI = 2 } bl_role_t;

typedef enum { BL_ITEM_HEART = 1, BL_ITEM_STAR, BL_ITEM_BOMB, BL_ITEM_COUNT } bl_item_t;

typedef enum {
    BL_CAUGHT_LOVED = 1,  // a heart: it went all soft
    BL_CAUGHT_WOW,        // a star: starry-eyed
    BL_CAUGHT_KO,         // a bomb: knocked out
    BL_CAUGHT_BUSY,       // it could not look: a game, a menu, a question
} bl_caught_t;

typedef enum { BL_NO_BUSY = 1, BL_NO_DECLINED, BL_NO_TIMEOUT } bl_no_t;

typedef struct {
    uint8_t type;  // bl_type_t
    uint8_t seq;   // 0: sent once, never filtered; else the sender's counter (bl_tx_t)
    uint8_t role;  // the sender's bl_role_t
    int32_t a, b, c;
    uint8_t d, e;
    char name[8];  // NUL-padded, not necessarily terminated
} bl_msg_t;

// Into exactly BL_FRAME_BYTES. Returns BL_FRAME_BYTES.
size_t bl_encode(const bl_msg_t *m, uint8_t out[BL_FRAME_BYTES]);
// False for anything that is not one of our frames: the wrong length, magic,
// version or type. Other people's ESP-NOW traffic is not ours to read.
bool bl_decode(const uint8_t *data, size_t len, bl_msg_t *m);

// --- Resending ------------------------------------------------------------
//
// A queue of frames that must arrive, sent one at a time, in order. The glue
// asks bl_tx_due() what to put on the air, and tells bl_tx_result() what the
// radio's send callback said. A frame is resent every BL_RETRY_MS until it is
// acknowledged or its time runs out.

#define BL_TX_SLOTS 6
#define BL_RETRY_MS 12
// No verdict from the radio this long after a send: count it as not acked.
#define BL_RESULT_MS 120
#define BL_GIVE_UP_MS 1500  // the default for bl_tx_push

typedef struct {
    bl_msg_t q[BL_TX_SLOTS];
    uint32_t until[BL_TX_SLOTS];  // give up on it then
    uint8_t head, count;
    uint8_t seq;  // the last one handed out; never 0
    bool in_flight;
    uint32_t sent_at, next_try;
} bl_tx_t;

typedef enum { BL_TX_PENDING = 0, BL_TX_DELIVERED, BL_TX_FAILED } bl_tx_outcome_t;

// The seed sets the first sequence number, so a sender that restarts does not
// reuse the numbers the other side has just filtered.
void bl_tx_init(bl_tx_t *tx, uint32_t seed);
// Queued behind anything already waiting, and given its sequence number.
// False when the queue is full (the frame is dropped).
bool bl_tx_push(bl_tx_t *tx, const bl_msg_t *m, uint32_t give_up_ms, uint32_t now_ms);
// The frame to send now, if any: the oldest, when no send is waiting for its
// verdict and its retry is due. Marks it in flight. A frame whose time ran
// out while it waited comes back from bl_tx_expired() instead.
bool bl_tx_due(bl_tx_t *tx, uint32_t now_ms, bl_msg_t *out);
// The radio's verdict on the frame bl_tx_due() gave out last. DELIVERED or
// FAILED take it off the queue and copy it to *done; PENDING means it will be
// sent again.
bl_tx_outcome_t bl_tx_result(bl_tx_t *tx, bool acked, uint32_t now_ms, bl_msg_t *done);
// The oldest frame, when its time has run out with no send in flight: taken
// off the queue and copied to *done. Call until false.
bool bl_tx_expired(bl_tx_t *tx, uint32_t now_ms, bl_msg_t *done);
static inline bool bl_tx_idle(const bl_tx_t *tx) { return tx->count == 0; }

// --- Duplicates -------------------------------------------------------------
//
// A frame whose acknowledgement was lost is sent again and arrives twice.
// The receiver remembers the last few sequence numbers it took.

#define BL_RX_SEEN 8

typedef struct {
    uint8_t seen[BL_RX_SEEN];
    uint8_t next;
} bl_rx_t;

void bl_rx_init(bl_rx_t *rx);
// False when this frame was taken already. seq 0 is never a duplicate.
bool bl_rx_fresh(bl_rx_t *rx, uint8_t seq);

// --- Near or not --------------------------------------------------------------

#define BL_NEAR_MS 45000         // heard within this: near (YETI asks every 15 s while near)
#define BL_ARRIVAL_GAP_MS 120000  // away at least this long, then heard: it has arrived

typedef struct {
    bool heard;  // ever
    uint32_t heard_at;
} bl_presence_t;

void bl_presence_init(bl_presence_t *p);
// The other one was heard (a frame came, or one of ours was acknowledged).
// True when that is an arrival: the first time, or after BL_ARRIVAL_GAP_MS
// of silence - the moment to say hello.
bool bl_presence_note(bl_presence_t *p, uint32_t now_ms);
bool bl_presence_near(const bl_presence_t *p, uint32_t now_ms);
