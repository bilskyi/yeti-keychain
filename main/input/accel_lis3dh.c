// LIS3DH accelerometer on the shared I2C bus, INT1 on GPIO0.
//
// Taps come from the chip's click unit: single click on all axes, latched,
// routed to INT1 (active high). Ringing and double taps are sorted out in
// software (motion_click), which proved more reliable than the chip's own
// double-click detection. INT1 has a high-level interrupt that
// doubles as the light-sleep wake-up, so a tap wakes the chip and kicks the
// main loop. poll() reads CLICK_SRC every frame (which clears the latch) and
// unmasks INT1 once it is low again.
//
// Shake and tilt are software (motion_logic.c) on samples read in poll():
// one per frame, so 5 Hz while still and ~30 Hz once moving.
//
// 200 Hz normal mode, +-4 g, ~20 uA. Deep sleep: INT1 high wakes the chip.

#include "config.h"

#if FEATURE_ACCEL

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "i2c_bus.h"
#include "input/input.h"
#include "input/motion_logic.h"
#include "input/orient.h"

static const char *TAG = "lis3dh";

#define REG_WHO_AM_I 0x0F
#define WHO_AM_I_LIS3DH 0x33
#define REG_CTRL1 0x20
#define REG_CTRL2 0x21
#define REG_CTRL3 0x22
#define REG_CTRL4 0x23
#define REG_CTRL5 0x24
#define REG_CTRL6 0x25
#define REG_REFERENCE 0x26
#define REG_OUT_X_L 0x28
#define REG_CLICK_CFG 0x38
#define REG_CLICK_SRC 0x39
#define REG_CLICK_THS 0x3A
#define REG_TIME_LIMIT 0x3B
#define AUTO_INCREMENT 0x80

#define CLICK_SRC_SCLICK 0x10

#define I2C_TIMEOUT_MS 20
#define MG_PER_LSB 8  // normal (10-bit) mode at +-4 g

static i2c_master_dev_handle_t s_dev;
static motion_t s_motion;
static volatile bool s_masked;
static bool s_ready;
static int s_last_mg[3];   // screen frame
static int s_last_raw[3];  // board frame
static bool s_has_sample;
static int8_t s_orient[3];  // input/orient.h; all zero = board axes as they are

static esp_err_t write_reg(uint8_t reg, uint8_t val) {
    const uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

static esp_err_t read_regs(uint8_t reg, uint8_t *out, size_t n) {
    if (n > 1) reg |= AUTO_INCREMENT;
    return i2c_master_transmit_receive(s_dev, &reg, 1, out, n, I2C_TIMEOUT_MS);
}

static void IRAM_ATTR on_int1(void *arg) {
    (void)arg;
    gpio_intr_disable(PIN_ACCEL_INT1);  // level interrupt: high until CLICK_SRC is read
    s_masked = true;
    input_kick_from_isr();
}

static esp_err_t find_chip(i2c_master_bus_handle_t bus) {
    static const uint8_t ADDRS[] = {0x18, 0x19};  // SDO/SA0 low or high
    for (size_t i = 0; i < sizeof(ADDRS); i++) {
        if (i2c_master_probe(bus, ADDRS[i], I2C_TIMEOUT_MS) != ESP_OK) continue;
        const i2c_device_config_t dev = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = ADDRS[i],
            .scl_speed_hz = DISPLAY_I2C_HZ,
        };
        if (i2c_master_bus_add_device(bus, &dev, &s_dev) != ESP_OK) continue;
        uint8_t who = 0;
        if (read_regs(REG_WHO_AM_I, &who, 1) == ESP_OK && who == WHO_AM_I_LIS3DH) {
            ESP_LOGI(TAG, "LIS3DH at 0x%02X", ADDRS[i]);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "0x%02X answers but WHO_AM_I is 0x%02X, not 0x33", ADDRS[i], who);
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t configure(void) {
    static const uint8_t REGS[][2] = {
        {REG_CTRL1, 0x67},  // 200 Hz, normal mode, X/Y/Z on
        {REG_CTRL2, 0x04},  // high-pass filter on the click path only (ignore gravity)
        {REG_CTRL3, 0x80},  // click -> INT1
        {REG_CTRL4, 0x90},  // block data update, +-4 g
        {REG_CTRL5, 0x00},
        {REG_CTRL6, 0x00},  // interrupts active high
        {REG_CLICK_CFG, 0x15},  // single click on X, Y, Z
        {REG_CLICK_THS, 0x80 | ACCEL_TAP_THS},  // bit 7: latch until CLICK_SRC is read
        {REG_TIME_LIMIT, ACCEL_TAP_LIMIT},
    };
    for (size_t i = 0; i < sizeof(REGS) / sizeof(REGS[0]); i++) {
        const esp_err_t err = write_reg(REGS[i][0], REGS[i][1]);
        if (err != ESP_OK) return err;
    }
    uint8_t junk;
    read_regs(REG_REFERENCE, &junk, 1);  // resets the high-pass filter
    read_regs(REG_CLICK_SRC, &junk, 1);  // drops a click latched before this boot
    return ESP_OK;
}

static esp_err_t setup_int1(void) {
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_ACCEL_INT1,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // the LIS3DH drives it push-pull
        .intr_type = GPIO_INTR_HIGH_LEVEL,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err == ESP_OK) err = input_gpio_isr_init();
    if (err == ESP_OK) {
        gpio_sleep_sel_dis(PIN_ACCEL_INT1);
        err = gpio_isr_handler_add(PIN_ACCEL_INT1, on_int1, NULL);
    }
    if (err == ESP_OK) err = gpio_wakeup_enable(PIN_ACCEL_INT1, GPIO_INTR_HIGH_LEVEL);
    return err;
}

static esp_err_t init(void) {
    motion_init(&s_motion);
    i2c_master_bus_handle_t bus = i2c_bus_handle();
    if (!bus) return ESP_ERR_INVALID_STATE;
    esp_err_t err = find_chip(bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no LIS3DH at 0x18/0x19: check VCC, SDA GPIO%d, SCL GPIO%d", PIN_I2C_SDA,
                 PIN_I2C_SCL);
        return err;
    }
    err = configure();
    if (err == ESP_OK) err = setup_int1();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "setup failed: %s", esp_err_to_name(err));
        return err;
    }
    s_ready = true;
    return ESP_OK;
}

static void read_click(uint32_t now_ms) {
    uint8_t src = 0;
    if (read_regs(REG_CLICK_SRC, &src, 1) != ESP_OK) return;
    if (src & CLICK_SRC_SCLICK) {
        const input_event_t ev = motion_click(&s_motion, now_ms);
        if (ev != INPUT_NONE) input_post(ev);
    }
    const input_event_t tap = motion_tap_poll(&s_motion, now_ms);
    if (tap != INPUT_NONE) input_post(tap);
}

static void read_motion(uint32_t now_ms) {
    uint8_t b[6];
    if (read_regs(REG_OUT_X_L, b, sizeof(b)) != ESP_OK) return;
    int mg[3];
    for (int i = 0; i < 3; i++) {
        // Left-justified 10-bit: shift the low 6 junk bits out, keeping the sign.
        mg[i] = ((int16_t)(b[2 * i] | (b[2 * i + 1] << 8)) >> 6) * MG_PER_LSB;
    }
    for (int i = 0; i < 3; i++) s_last_raw[i] = mg[i];
    orient_apply(s_orient, s_last_raw, mg);
    for (int i = 0; i < 3; i++) s_last_mg[i] = mg[i];
    s_has_sample = true;
    const input_event_t ev = motion_update(&s_motion, mg[0], mg[1], mg[2], now_ms);
    if (ev != INPUT_NONE) input_post(ev);
}

static void poll(uint32_t now_ms) {
    if (!s_ready) return;
    // Read the latched click every frame, not only when INT1 says so: taps
    // still work with INT1 unwired or flaky. INT1 is then only the fast path
    // (kicks the loop at once) and the wake-up.
    read_click(now_ms);
    read_motion(now_ms);
    if (s_masked && !gpio_get_level(PIN_ACCEL_INT1)) {
        s_masked = false;
        gpio_intr_enable(PIN_ACCEL_INT1);
    }
}

static bool busy(uint32_t now_ms) {
    return s_ready && (s_masked || motion_busy(&s_motion, now_ms));
}

static void arm_wakeup(void) {
    if (!s_ready) return;
    esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_ACCEL_INT1, ESP_GPIO_WAKEUP_GPIO_HIGH);
}

bool input_accel_mg(int mg[3]) {
    if (!s_has_sample) return false;
    for (int i = 0; i < 3; i++) mg[i] = s_last_mg[i];
    return true;
}

bool input_accel_raw_mg(int mg[3]) {
    if (!s_has_sample) return false;
    for (int i = 0; i < 3; i++) mg[i] = s_last_raw[i];
    return true;
}

void input_accel_set_orient(const int8_t map[3]) {
    for (int i = 0; i < 3; i++) s_orient[i] = map[i];
}

const input_source_t input_source_accel = {
    .name = "lis3dh",
    .init = init,
    .poll = poll,
    .arm_wakeup = arm_wakeup,
    .busy = busy,
};

#else

#include <stdbool.h>

#include "input/input.h"

bool input_accel_mg(int mg[3]) {
    (void)mg;
    return false;
}

bool input_accel_raw_mg(int mg[3]) {
    (void)mg;
    return false;
}

void input_accel_set_orient(const int8_t map[3]) {
    (void)map;
}

#endif
