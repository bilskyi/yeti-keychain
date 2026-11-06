// The radio. The single biggest power draw on the C3, so it is only brought
// up for what needs it: BIT (link/buddy.h), and an update over WiFi
// (net/update.h). Both share one bring-up, here.

#pragma once

#include <stdbool.h>

// The flags' placeholders (WiFi time sync, BLE), as before.
void net_init(void);

// WiFi up as a station with nothing joined, in minimum modem sleep: ready for
// ESP-NOW, or to become an access point. Once; later calls say how the first
// went. False when it could not start.
bool net_radio_up(void);

// The ESP32-C3 Super Mini's antenna is badly matched: at full power what it
// sends comes out too garbled for anyone to hear, though it receives fine.
// 8.5 dBm (the unit is 0.25 dBm) is the usual cure, and on the bench it took
// the keychain from not one frame reaching BIT to every one, acknowledged in
// 2-4 ms. Set again after anything that may reset it (a mode change).
#define NET_TX_POWER_QDBM 34
void net_tx_power(void);
