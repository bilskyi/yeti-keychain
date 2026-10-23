// The 1.3" 128x64 OLED: SH1106 or SSD1306, picked by DISPLAY_DRIVER in
// config.h. One panel, so module-level state rather than a handle.
//
// Written rather than pulled in (no U8g2): it is an init table and a flush,
// and owning it lets the flush send only what changed. While the face is
// still, that is nothing at all, so the I2C bus and CPU stay asleep.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "display/gfx.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

// Probes 0x3C then 0x3D, initialises the panel, clears it, turns it on.
// ESP_ERR_NOT_FOUND when nothing answers: the caller logs it and carries on,
// and every other call becomes a no-op.
esp_err_t display_init(i2c_master_bus_handle_t bus, uint8_t contrast);

// Sends, per 8-row page, only the column range that differs from what the
// panel already shows.
esp_err_t display_flush(const gfx_t *g);

esp_err_t display_set_contrast(uint8_t contrast);

// Panel sleep (0xAE: charge pump and driver off, RAM kept) or wake.
esp_err_t display_power(bool on);

bool display_ready(void);
