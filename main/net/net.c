#include "net/net.h"

#include "config.h"
#include "esp_log.h"

#if FEATURE_BUDDY || FEATURE_UPDATE
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#endif

void net_init(void) {
#if FEATURE_WIFI
    // TODO: WiFi station for SNTP time (would replace the night stub).
    ESP_LOGW("net", "FEATURE_WIFI: placeholder, radio not started");
#endif
#if FEATURE_BLE
    // TODO: BLE (NimBLE) for phone time sync / notifications.
    ESP_LOGW("net", "FEATURE_BLE: placeholder, radio not started");
#endif
}

void net_tx_power(void) {
#if FEATURE_BUDDY || FEATURE_UPDATE
    const esp_err_t err = esp_wifi_set_max_tx_power(NET_TX_POWER_QDBM);
    if (err != ESP_OK) ESP_LOGW("net", "tx power: %s", esp_err_to_name(err));
#endif
}

bool net_radio_up(void) {
#if FEATURE_BUDDY || FEATURE_UPDATE
    static int state;  // 0 not tried, 1 up, -1 failed
    if (state != 0) return state > 0;
    esp_err_t err = esp_netif_init();
    if (err == ESP_OK) err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE) err = ESP_OK;  // made already
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (err == ESP_OK) err = esp_wifi_init(&cfg);
    if (err == ESP_OK) err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err == ESP_OK) err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (err == ESP_OK) net_tx_power();
    if (err != ESP_OK) {
        ESP_LOGE("net", "radio: %s", esp_err_to_name(err));
        state = -1;
        return false;
    }
    state = 1;
    return true;
#else
    return false;
#endif
}
