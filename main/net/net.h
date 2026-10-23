// Wireless placeholders. The radio is the single biggest power draw on the C3,
// so nothing here starts unless its flag is on, and in v0.1 even then it only
// logs. Real WiFi (OTA, time sync) and BLE (phone link) come later.

#pragma once

void net_init(void);
