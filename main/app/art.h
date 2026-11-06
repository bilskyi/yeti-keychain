// A picture for each game (the games list) and each song (the jukebox),
// drawn in the top of the screen, y 0..38. Pure C (host-tested).

#pragma once

#include "app/app.h"
#include "display/gfx.h"

void art_game(gfx_t *g, app_item_t it);
void art_song(gfx_t *g, const char *name);  // by name; quavers for one it doesn't know
