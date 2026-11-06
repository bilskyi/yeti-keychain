// The radio to BIT, the desk robot: ESP-NOW (link/bitlink.h says what is
// said, and why YETI is the one that asks).
//
// BIT sits on its router's channel and says HELLO there every second. YETI
// does not know the channel until it has heard one, so the first time it hops
// through them listening - only while the BIT or Pong screen is up, since the
// radio listening costs more than everything else on the keychain together.
// What it finds (BIT's address and channel) is kept in NVS, and from then on
// it pings BIT while awake - every 15 s when near, backing off to every two
// minutes when not, and at once when the keychain is handled: the
// acknowledgement alone says BIT is near.
//
// FEATURE_BUDDY off: every call here is a stub that says "no radio".

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "link/bitlink.h"

typedef enum {
    BUDDY_OFF = 0,     // no radio
    BUDDY_SEARCHING,   // hopping the channels for BIT's HELLO
    BUDDY_AWAY,        // BIT is known, but not heard lately
    BUDDY_NEAR,
} buddy_status_t;

#if FEATURE_BUDDY

// After storage_load (NVS is up). Starts the radio and the buddy task.
void buddy_init(void);
// Awake: ping BIT now and then. Asleep (night): the radio rests.
void buddy_set_awake(bool awake);
// The radio is wanted for something else (an update over WiFi): leave it
// alone - no sends, no channel hopping - until unpaused.
void buddy_pause(bool paused);
// Someone is handling the keychain: ask after BIT now, if it has not been
// asked lately.
void buddy_nudge(void);
// Keep the radio listening, and search for BIT if it is lost: while a screen
// that talks to BIT is up. Off, it only wakes to send.
void buddy_listen(bool on);

// To BIT. `reliable`: resent until acknowledged; its fate comes back from
// buddy_take_done(). False when not queued (no BIT known, queue full).
bool buddy_send(const bl_msg_t *m, bool reliable);
// The next frame from BIT, if any.
bool buddy_recv(bl_msg_t *m);
// The next reliable frame that was delivered or given up on.
bool buddy_take_done(bl_msg_t *m, bool *delivered);

buddy_status_t buddy_status(void);
uint32_t buddy_heard_at(void);  // ms; 0 never
// True once for each arrival (bitlink.h): BIT is back after a long while.
bool buddy_take_arrival(void);

#else

static inline void buddy_init(void) {}
static inline void buddy_set_awake(bool awake) { (void)awake; }
static inline void buddy_pause(bool paused) { (void)paused; }
static inline void buddy_nudge(void) {}
static inline void buddy_listen(bool on) { (void)on; }
static inline bool buddy_send(const bl_msg_t *m, bool reliable) { (void)m; (void)reliable; return false; }
static inline bool buddy_recv(bl_msg_t *m) { (void)m; return false; }
static inline bool buddy_take_done(bl_msg_t *m, bool *delivered) { (void)m; (void)delivered; return false; }
static inline buddy_status_t buddy_status(void) { return BUDDY_OFF; }
static inline uint32_t buddy_heard_at(void) { return 0; }
static inline bool buddy_take_arrival(void) { return false; }

#endif
