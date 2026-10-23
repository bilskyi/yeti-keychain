// The one I2C bus (SDA GPIO6, SCL GPIO7). The OLED is on it now; the LIS3DH
// will join it later, which is why the bus is not owned by the display.

#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t i2c_bus_init(void);
i2c_master_bus_handle_t i2c_bus_handle(void);  // NULL until init succeeds
