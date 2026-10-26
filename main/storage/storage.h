// The one saved record (best scores, settings) in NVS, so it survives power-off.

#pragma once

#include "app/app.h"

// Fills *s from flash, or zeroes it when nothing is saved yet (or flash fails).
void storage_load(app_save_t *s);
void storage_save(const app_save_t *s);
