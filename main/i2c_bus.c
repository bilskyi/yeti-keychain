#include "i2c_bus.h"

#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "i2c";
static i2c_master_bus_handle_t s_bus;

esp_err_t i2c_bus_init(void) {
    if (s_bus) return ESP_OK;
    // An idle bus sits high. A line held low is a short to GND, a crossed
    // wire or an unpowered module clamping it: nothing on it will answer.
    const gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_I2C_SDA) | (1ULL << PIN_I2C_SCL),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&in);
    gpio_set_level(PIN_I2C_SDA, 1);
    gpio_set_level(PIN_I2C_SCL, 1);
    esp_rom_delay_us(100);
    const int sda = gpio_get_level(PIN_I2C_SDA), scl = gpio_get_level(PIN_I2C_SCL);
    if (!sda || !scl) {
        ESP_LOGE(TAG, "bus held low: SDA(GPIO%d)=%d SCL(GPIO%d)=%d, expected 1/1", PIN_I2C_SDA, sda,
                 PIN_I2C_SCL, scl);
    } else {
        // Neighbouring pins: a solder bridge ties them together.
        gpio_set_level(PIN_I2C_SDA, 0);
        esp_rom_delay_us(100);
        if (!gpio_get_level(PIN_I2C_SCL)) {
            ESP_LOGE(TAG, "SDA(GPIO%d) and SCL(GPIO%d) are shorted together: look for a solder bridge",
                     PIN_I2C_SDA, PIN_I2C_SCL);
        }
        gpio_set_level(PIN_I2C_SDA, 1);
    }
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
