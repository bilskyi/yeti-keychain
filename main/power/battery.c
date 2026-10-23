#include "power/battery.h"

#include "config.h"

#if !FEATURE_BATTERY_ADC

void battery_init(void) {}

battery_status_t battery_read(void) {
    return (battery_status_t){.level = BATTERY_UNKNOWN, .mv = -1, .percent = -1};
}

#else

// GPIO4 = ADC1 channel 4 on the C3, behind a 1M/1M divider with 100 nF.
// The divider's 500k source impedance is fine for the ADC only because the
// 100 nF cap holds the node: sample rarely, and average a few reads.
//
// UNVERIFIED on hardware: the divider is not wired yet.

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "battery";

#define SAMPLES 8

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static battery_level_t s_level = BATTERY_UNKNOWN;

void battery_init(void) {
    const adc_oneshot_unit_init_cfg_t unit = {.unit_id = ADC_UNIT_1};
    if (adc_oneshot_new_unit(&unit, &s_adc) != ESP_OK) {
        ESP_LOGE(TAG, "ADC1 init failed");
        s_adc = NULL;
        return;
    }
    // 12 dB: ~0-2.5 V usable on the C3; a full cell through the divider is 2.1 V.
    const adc_oneshot_chan_cfg_t chan = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    adc_oneshot_config_channel(s_adc, ADC_CHANNEL_4, &chan);

    const adc_cali_curve_fitting_config_t cali = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_4,
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
        if (adc_oneshot_read(s_adc, ADC_CHANNEL_4, &raw) != ESP_OK) continue;
        if (s_cali && adc_cali_raw_to_voltage(s_cali, raw, &mv) == ESP_OK) {
            sum += mv;
        } else {
            sum += raw * 2500 / 4095;  // uncalibrated fallback
        }
        n++;
    }
    if (!n) return st;

    st.mv = sum / n * BATTERY_DIVIDER_RATIO;
    st.percent = battery_percent_from_mv(st.mv);
    st.level = s_level = battery_level_from_mv(st.mv, s_level);
    ESP_LOGI(TAG, "%d mV, %d %%", st.mv, st.percent);
    return st;
}

#endif
