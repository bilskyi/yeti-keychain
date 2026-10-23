#include "net/net.h"

#include "config.h"
#include "esp_log.h"

void net_init(void) {
#if FEATURE_WIFI
    // TODO: WiFi station for OTA and SNTP time (would replace the night stub).
    ESP_LOGW("net", "FEATURE_WIFI: placeholder, radio not started");
#endif
#if FEATURE_BLE
    // TODO: BLE (NimBLE) for phone time sync / notifications.
    ESP_LOGW("net", "FEATURE_BLE: placeholder, radio not started");
#endif
}
