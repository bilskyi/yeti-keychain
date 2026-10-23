#include "display/display.h"

#include <string.h>

#include "config.h"
#include "esp_log.h"

static const char *TAG = "display";

// Every transfer starts with a control byte: commands follow, or pixel data.
#define CTRL_CMD 0x00
#define CTRL_DATA 0x40

#define I2C_TIMEOUT_MS 50

#if DISPLAY_DRIVER == DISPLAY_SH1106
// The SH1106 has 132 columns of RAM and 1.3" modules wire the middle 128,
// so column 0 of the image is RAM column 2. Forgetting this is the classic
// "shifted by 2 px with garbage at the edge" symptom.
#define COL_OFFSET 2
#define RAM_COLS 132
#elif DISPLAY_DRIVER == DISPLAY_SSD1306
#define COL_OFFSET 0
#define RAM_COLS 128
#else
#error "DISPLAY_DRIVER must be DISPLAY_SH1106 or DISPLAY_SSD1306"
#endif

// Power-on setup. Both controllers understand the page-addressing commands
// the flush uses (0xB0+page, 0x00/0x10 column), so only the power section
// differs.
static const uint8_t INIT[] = {
    0xAE,        // display off while configuring
    0xD5, 0x80,  // clock divide / oscillator: datasheet default
    0xA8, 0x3F,  // multiplex: 64 rows
    0xD3, 0x00,  // no vertical offset
    0x40,        // start line 0
#if DISPLAY_DRIVER == DISPLAY_SH1106
    0xAD, 0x8B,  // DC-DC converter on (the SH1106's internal supply)
    0x32,        // pump voltage 8.0 V (reset default)
    0xDB, 0x35,  // VCOM deselect: reset default
#else
    0x8D, 0x14,  // charge pump on: without it the panel ACKs but stays dark
    0x20, 0x02,  // page addressing mode, same as the SH1106's only mode
    0xDB, 0x20,  // VCOM deselect: reset default
    0x2E,        // scrolling off
#endif
    0xA1,        // segment remap: mirror horizontally
    0xC8,        // COM scan reversed: mirror vertically (together: not upside down)
    0xDA, 0x12,  // COM pins: alternate, as every 128x64 module wires them
    0xD9, 0x22,  // pre-charge: reset default. Lower = dimmer, less current.
    0xA4,        // show RAM (not all-on)
    0xA6,        // normal, not inverted: white pixels on black
};

static struct {
    i2c_master_dev_handle_t dev;
    bool ready;
    bool shadow_valid;
    uint8_t shadow[GFX_FB_BYTES];       // what the panel currently shows
    uint8_t scratch[1 + RAM_COLS];      // control byte + one page row
} s;

static esp_err_t cmds(const uint8_t *c, size_t n) {
    uint8_t buf[1 + 8];
    while (n) {
        const size_t chunk = n < 8 ? n : 8;
        buf[0] = CTRL_CMD;
        memcpy(&buf[1], c, chunk);
        const esp_err_t err = i2c_master_transmit(s.dev, buf, 1 + chunk, I2C_TIMEOUT_MS);
        if (err != ESP_OK) return err;
        c += chunk;
        n -= chunk;
    }
    return ESP_OK;
}

static esp_err_t set_cursor(int page, int col) {
    const int c = col + COL_OFFSET;
    const uint8_t seq[] = {
        (uint8_t)(0xB0 | page),
        (uint8_t)(0x00 | (c & 0x0F)),
        (uint8_t)(0x10 | (c >> 4)),
    };
    return cmds(seq, sizeof(seq));
}

static esp_err_t clear_ram(void) {
    // All RAM_COLS, including the SH1106's two hidden columns each side, so
    // nothing left over from a previous run can show at the edges.
    memset(s.scratch, 0, sizeof(s.scratch));
    s.scratch[0] = CTRL_DATA;
    for (int page = 0; page < GFX_PAGES; page++) {
        const uint8_t seq[] = {(uint8_t)(0xB0 | page), 0x00, 0x10};
        esp_err_t err = cmds(seq, sizeof(seq));
        if (err == ESP_OK) err = i2c_master_transmit(s.dev, s.scratch, 1 + RAM_COLS, I2C_TIMEOUT_MS);
        if (err != ESP_OK) return err;
    }
    memset(s.shadow, 0, sizeof(s.shadow));
    s.shadow_valid = true;
    return ESP_OK;
}

esp_err_t display_init(i2c_master_bus_handle_t bus, uint8_t contrast) {
    s.ready = false;
    if (!bus) return ESP_ERR_INVALID_STATE;

    static const uint8_t ADDRS[] = {DISPLAY_I2C_ADDR, DISPLAY_I2C_ADDR == 0x3C ? 0x3D : 0x3C};
    uint8_t addr = 0;
    for (size_t i = 0; i < sizeof(ADDRS); i++) {
        if (i2c_master_probe(bus, ADDRS[i], I2C_TIMEOUT_MS) == ESP_OK) {
            addr = ADDRS[i];
            break;
        }
    }
    if (!addr) {
        ESP_LOGE(TAG, "no OLED at 0x3C/0x3D: check VCC/GND/SDA(GPIO%d)/SCL(GPIO%d)",
                 PIN_I2C_SDA, PIN_I2C_SCL);
        return ESP_ERR_NOT_FOUND;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = DISPLAY_I2C_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s.dev);
    if (err == ESP_OK) err = cmds(INIT, sizeof(INIT));
    if (err == ESP_OK) err = clear_ram();
    if (err == ESP_OK) {
        const uint8_t c[] = {0x81, contrast, 0xAF};  // contrast, display on
        err = cmds(c, sizeof(c));
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init at 0x%02X failed: %s", addr, esp_err_to_name(err));
        return err;
    }
    s.ready = true;
    ESP_LOGI(TAG, "%s at 0x%02X, contrast %u",
             DISPLAY_DRIVER == DISPLAY_SH1106 ? "SH1106" : "SSD1306", addr, contrast);
    return ESP_OK;
}

esp_err_t display_flush(const gfx_t *g) {
    if (!s.ready) return ESP_ERR_INVALID_STATE;
    for (int page = 0; page < GFX_PAGES; page++) {
        const uint8_t *src = &g->buf[page * GFX_W];
        uint8_t *shadow = &s.shadow[page * GFX_W];

        // Narrowest column range that changed on this page.
        int lo = 0, hi = GFX_W - 1;
        if (s.shadow_valid) {
            while (lo < GFX_W && src[lo] == shadow[lo]) lo++;
            if (lo == GFX_W) continue;  // page unchanged
            while (src[hi] == shadow[hi]) hi--;
        }

        const int n = hi - lo + 1;
        s.scratch[0] = CTRL_DATA;
        memcpy(&s.scratch[1], &src[lo], n);
        esp_err_t err = set_cursor(page, lo);
        if (err == ESP_OK) err = i2c_master_transmit(s.dev, s.scratch, 1 + n, I2C_TIMEOUT_MS);
        if (err != ESP_OK) {
            s.shadow_valid = false;  // unknown state: resend everything next time
            return err;
        }
        memcpy(&shadow[lo], &src[lo], n);
    }
    s.shadow_valid = true;
    return ESP_OK;
}

esp_err_t display_set_contrast(uint8_t contrast) {
    if (!s.ready) return ESP_ERR_INVALID_STATE;
    const uint8_t c[] = {0x81, contrast};
    return cmds(c, sizeof(c));
}

esp_err_t display_power(bool on) {
    if (!s.ready) return ESP_ERR_INVALID_STATE;
    const uint8_t c = on ? 0xAF : 0xAE;
    return cmds(&c, 1);
}

bool display_ready(void) {
    return s.ready;
}
