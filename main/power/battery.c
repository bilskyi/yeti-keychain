#include "power/battery.h"

#include "config.h"

#include "driver/gpio.h"

#if FEATURE_CHARGE_SENSE
static bool s_sense_ready;

bool battery_cable_in(void) {
    if (!s_sense_ready) {
        gpio_config_t io = {.pin_bit_mask = 1ULL << PIN_CHARGE_SENSE, .mode = GPIO_MODE_INPUT,
                            .pull_down_en = GPIO_PULLDOWN_ENABLE};
        s_sense_ready = gpio_config(&io) == ESP_OK;
    }
    return s_sense_ready && gpio_get_level(PIN_CHARGE_SENSE);
}
#else
bool battery_cable_in(void) {
    return false;
}
#endif

#if !FEATURE_BATTERY_ADC

void battery_init(void) {}

battery_status_t battery_read(void) {
    return (battery_status_t){.level = BATTERY_UNKNOWN, .mv = -1, .percent = -1};
}

#else

// PIN_BATTERY_ADC on ADC1, behind a 5.1k/5.1k divider from the 5V pin: half
// the cell voltage, from a source low enough for the ADC without a cap.
// The mic uses ADC1 in DMA mode; while it runs, a one-shot read is refused
// and battery_read() returns mv -1 (main.c pauses the mic and retries).

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "battery";

#define SAMPLES 8

static adc_oneshot_unit_handle_t s_adc;
static adc_channel_t s_channel;
static adc_cali_handle_t s_cali;
static battery_level_t s_level = BATTERY_UNKNOWN;

void battery_init(void) {
    adc_unit_t unit_id;
    if (adc_oneshot_io_to_channel(PIN_BATTERY_ADC, &unit_id, &s_channel) != ESP_OK || unit_id != ADC_UNIT_1) {
        ESP_LOGE(TAG, "GPIO%d is not an ADC1 pin", PIN_BATTERY_ADC);
        return;
    }
    const adc_oneshot_unit_init_cfg_t unit = {.unit_id = ADC_UNIT_1};
    if (adc_oneshot_new_unit(&unit, &s_adc) != ESP_OK) {
        ESP_LOGE(TAG, "ADC1 init failed");
        s_adc = NULL;
        return;
    }
    // 12 dB: ~0-2.5 V usable on the C3; a full cell through the divider is
    // 2.1 V, USB's ~4.8 V is 2.4 V.
    const adc_oneshot_chan_cfg_t chan = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    adc_oneshot_config_channel(s_adc, s_channel, &chan);

    const adc_cali_curve_fitting_config_t cali = {
        .unit_id = ADC_UNIT_1,
        .chan = s_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali, &s_cali) != ESP_OK) {
        ESP_LOGW(TAG, "no eFuse calibration; readings will be rough");
        s_cali = NULL;
    }
}

battery_status_t battery_read(void) {
    battery_status_t st = {.level = BATTERY_UNKNOWN, .mv = -1, .percent = -1};
    if (!s_adc) return st;

    int sum = 0, n = 0;
    for (int i = 0; i < SAMPLES; i++) {
        int raw, mv;
        if (adc_oneshot_read(s_adc, s_channel, &raw) != ESP_OK) continue;  // busy: the mic
        if (s_cali && adc_cali_raw_to_voltage(s_cali, raw, &mv) == ESP_OK) {
            sum += mv;
        } else {
            sum += raw * 2500 / 4095;  // uncalibrated fallback
        }
        n++;
    }
    if (!n) return st;

    st.mv = sum / n * BATTERY_DIVIDER_RATIO;
    st.level = s_level = battery_level_from_mv(st.mv, s_level);
    st.percent = st.level == BATTERY_UNKNOWN ? -1 : battery_percent_from_mv(st.mv);
    ESP_LOGI(TAG, "%d mV, %d %%%s", st.mv, st.percent, st.level == BATTERY_UNKNOWN ? " (USB)" : "");
    return st;
}

#endif
