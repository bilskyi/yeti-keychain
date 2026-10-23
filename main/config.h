// Everything a person might want to change lives here: pins, which panel is
// fitted, feature flags, and the timing of the face and the power policy.
//
// Plain macros only, no ESP-IDF headers, so the host tests can include it too.
//
// Every flag can be overridden from the build without editing this file:
//   idf.py -B build-allon -DKC_FEATURES="FEATURE_BUTTONS=1;FEATURE_ACCEL=1" build

#pragma once

// ---------------------------------------------------------------------------
// Display driver. 1.3" modules are almost always SH1106. If the picture is
// shifted ~2 px sideways with a garbage column at one edge, or is garbled,
// the panel is an SSD1306: change this one line.
// ---------------------------------------------------------------------------
#define DISPLAY_SH1106 1
#define DISPLAY_SSD1306 2

#ifndef DISPLAY_DRIVER
#define DISPLAY_DRIVER DISPLAY_SH1106
#endif

#define DISPLAY_I2C_ADDR 0x3C  // the other common strap is 0x3D; init probes both
#define DISPLAY_I2C_HZ 400000

// ---------------------------------------------------------------------------
// Pins (fixed wiring, see docs/WIRING.md). Do not change without re-wiring.
// GPIO2, GPIO8, GPIO9 are strapping pins (GPIO8 also drives the onboard LED):
// never use them. Only GPIO0-5 can wake the C3 from deep sleep.
// ---------------------------------------------------------------------------
#define PIN_I2C_SDA 6
#define PIN_I2C_SCL 7

#define PIN_BTN_A 1  // to GND, internal pull-up
#define PIN_BTN_B 3
#define PIN_BTN_C 5
#define PIN_ACCEL_INT1 0     // LIS3DH INT1, same I2C bus as the OLED
#define PIN_BATTERY_ADC 4    // 1M/1M divider + 100 nF, ADC1 channel 4

// ---------------------------------------------------------------------------
// Feature flags. Display is always on; everything else defaults OFF.
// ---------------------------------------------------------------------------
#ifndef FEATURE_BUTTONS
#define FEATURE_BUTTONS 0
#endif
#ifndef FEATURE_ACCEL
#define FEATURE_ACCEL 0
#endif
#ifndef FEATURE_BATTERY_ADC
#define FEATURE_BATTERY_ADC 0
#endif
#ifndef FEATURE_WIFI
#define FEATURE_WIFI 0
#endif
#ifndef FEATURE_BLE
#define FEATURE_BLE 0
#endif

// ---------------------------------------------------------------------------
// Power policy
// ---------------------------------------------------------------------------
// Contrast 0-255. The panel's current scales with it; faces are readable at
// very low values indoors.
#define CONTRAST_DAY 0x30
#define CONTRAST_NIGHT 0x01
#define CONTRAST_LOWBAT 0x01

// Frame pacing. While something moves (blink, glance, yawn) we redraw at
// ~30 fps; while the face is still we only wake 5x a second.
#define FRAME_MS_ANIMATING 33
#define FRAME_MS_IDLE 200

// Burn-in: the whole face moves to the next offset of a small loop this often.
#define BURNIN_SHIFT_MS (3UL * 60UL * 1000UL)
#define BURNIN_MAX_SHIFT_PX 2

// Night mode stub. There is no wall clock yet, so night is a repeating cycle
// of uptime: NIGHT_CYCLE_AWAKE_MS awake, then NIGHT_CYCLE_NIGHT_MS of night.
// Any input event ends the current night early. Set NIGHT_CYCLE_NIGHT_MS to 0
// to disable. TODO: replace with real time (BLE/WiFi time sync or an RTC).
#define NIGHT_CYCLE_AWAKE_MS (30UL * 60UL * 1000UL)
#define NIGHT_CYCLE_NIGHT_MS (10UL * 60UL * 1000UL)

// Battery (only used with FEATURE_BATTERY_ADC).
#define BATTERY_SAMPLE_MS (30UL * 1000UL)
#define BATTERY_DIVIDER_RATIO 2  // 1M over 1M
#define BATTERY_MV_EMPTY 3300    // what we call 0 %; DW01 cuts off ~2.4 V
#define BATTERY_MV_FULL 4150
#define BATTERY_MV_LOW 3500      // show the icon as low
#define BATTERY_MV_CRITICAL 3350 // show the low-battery face and deep sleep

// How long the low-battery face stays up before the device deep-sleeps.
#define LOWBAT_FACE_MS 4000

// ---------------------------------------------------------------------------
// Face timing
// ---------------------------------------------------------------------------
#define BLINK_MIN_MS 2000
#define BLINK_MAX_MS 6000
#define BLINK_DURATION_MS 180

#define IDLE_BEHAVIOR_MIN_MS 6000   // gap between glances / yawns
#define IDLE_BEHAVIOR_MAX_MS 15000

#define MOOD_MIN_MS 8000            // how long a spontaneous mood holds
#define MOOD_MAX_MS 25000
