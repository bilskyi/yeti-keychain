// A new firmware over WiFi, for a keychain whose USB port is sealed away.
//
// Off until Settings > UPDATE asks for it. Then the keychain makes a WiFi of
// its own - YETI-7, with a fresh 8-digit password each time - and serves one
// page at 192.168.4.1 that takes a .bin. The image is written to the app slot
// that is not running (partitions.csv) and only chosen for the next boot once
// it is complete and verified; then the keychain restarts into it. A file
// that is not this project's firmware - BIT's, say - is refused before a byte
// of it is written.
//
// Leaving the screen ends it: the WiFi goes, the radio is BIT's again.

#pragma once

#include <stdbool.h>

#include "app/app.h"
#include "config.h"

#if FEATURE_UPDATE

// The WiFi and the page up. False when the radio would not start.
bool update_begin(void);
// Down again, an upload in progress abandoned (the running firmware stays).
void update_end(void);
// How it is going, for the screen.
void update_status(app_update_t *u);

#endif
