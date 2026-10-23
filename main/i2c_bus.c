#include "i2c_bus.h"

#include "config.h"
#include "esp_log.h"

static const char *TAG = "i2c";
static i2c_master_bus_handle_t s_bus;

esp_err_t i2c_bus_init(void) {
    if (s_bus) return ESP_OK;
    const i2c_master_bus_config_t cfg = {
        .i2c_port = -1,  // any free controller (the C3 has one)
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        // OLED modules carry their own 4.7-10k pull-ups; the internal ~45k
        // ones are a harmless backstop.
        .flags = {.enable_internal_pullup = true},
    };
    const esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bus init on SDA %d / SCL %d failed: %s", PIN_I2C_SDA, PIN_I2C_SCL,
                 esp_err_to_name(err));
        s_bus = NULL;
    }
    return err;
}

i2c_master_bus_handle_t i2c_bus_handle(void) {
    return s_bus;
}
