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
#define PIN_BUZZER 10        // passive buzzer, LEDC PWM; not a wake source
// Buzzer's other leg: -1 = on GND (current wiring; loud enough per Sasha).
// Set 20 and move the leg to GPIO20 for a bridge: the second pin is driven in
// antiphase, doubling the voltage across the buzzer with no transistor.
#define PIN_BUZZER_BRIDGE -1

// ---------------------------------------------------------------------------
// Feature flags. Display is always on. Buttons, LIS3DH and buzzer are fitted
// (Phase 2); a missing part only logs a warning at boot.
// ---------------------------------------------------------------------------
#ifndef FEATURE_BUTTONS
#define FEATURE_BUTTONS 1
#endif
#ifndef FEATURE_ACCEL
#define FEATURE_ACCEL 1
#endif
#ifndef FEATURE_SOUND
#define FEATURE_SOUND 1
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
// Buttons
// ---------------------------------------------------------------------------
#define BTN_DEBOUNCE_MS 25
#define BTN_LONG_MS 600  // LONG fires while still held, SHORT on release before this

// ---------------------------------------------------------------------------
// LIS3DH: tap in hardware (click unit, INT1), shake and tilt in software
// ---------------------------------------------------------------------------
// Click threshold in LSB of 31 mg (+-4 g range), after the high-pass filter.
// Lower = more sensitive. 20-40 is the usual range at +-4 g.
#define ACCEL_TAP_THS 24  // bench 2026-10-25: 30 missed finger taps, 20 too jumpy
#define ACCEL_TAP_LIMIT 16    // max tap length, 5 ms units at 200 Hz (80 ms)
// Taps are grouped in software (motion_logic.c): on the bench one finger tap
// made the board ring and the click unit fired 3-5 times, 30-60 ms apart, and
// its own double-click detection never triggered.
#define ACCEL_TAP_RING_MS 80     // a click this soon after the previous click is ringing
#define ACCEL_TAP_DOUBLE_MS 450  // a second tap within this of the first = DOUBLE_TAP
// Pressing a button jolts the board and the LIS3DH reads it as a tap: taps
// are dropped while a button is held and for this long after a button event.
#define TAP_AFTER_BUTTON_MS 600

// Shake: SHAKE_HITS samples whose |a| is more than SHAKE_DEV_MG away from 1 g,
// all within SHAKE_WINDOW_MS. Taps and tilt are ignored for a while after.
#define ACCEL_SHAKE_DEV_MG 900
#define ACCEL_SHAKE_HITS 4
#define ACCEL_SHAKE_WINDOW_MS 1000
#define ACCEL_SHAKE_COOLDOWN_MS 1500
// Any sample this far from 1 g keeps polling fast for a second, so a shake
// that starts between two idle frames is still sampled at ~30 Hz.
#define ACCEL_ACTIVE_DEV_MG 250

// Tilt: which board axis points left-right on the face once it is in the case
// (0 = X, 1 = Y, 2 = Z) and its sign. PLACEHOLDER until the board is mounted:
// if tilting left makes the eyes look right, flip the sign.
#define ACCEL_TILT_AXIS 0
#define ACCEL_TILT_SIGN 1
#define ACCEL_TILT_ENTER_MG 500  // ~30 degrees
#define ACCEL_TILT_EXIT_MG 250   // ~15 degrees, back to level
#define ACCEL_TILT_HOLD_MS 300

// ---------------------------------------------------------------------------
// Sound (FEATURE_SOUND): passive buzzer on PIN_BUZZER
// ---------------------------------------------------------------------------
// PWM duty in percent; 50 is loudest, lower is quieter (roughly).
#define SOUND_DUTY_PCT 50
// 1 for a 3-pin module with its own PNP transistor that beeps when I/O is low
// ("low level trigger"); 0 for a bare buzzer or one behind our NPN.
#define SOUND_ACTIVE_LOW 0
// GPIO drive strength 0-3 (~5 / 10 / 20 / 40 mA). The buzzer is wired straight
// to the pins with no transistor (Sasha has none), so the drive is what limits
// a coil buzzer's current. Bench 2026-10-25: 1 and 2 too quiet even at the
// buzzer's resonance; 3 is loud enough. Beeps are short, but this is the most
// a bare pin should do: a transistor is the real fix.
#define SOUND_DRIVE_CAP 3

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
// ...and when it wakes to try again. A brownout is not proof of an empty
// cell (slide-switch bounce at power-on can cause one), so sleeping forever
// would leave a full keychain dead until the switch is cycled. A really empty
// cell just browns out again on the retry, which costs about a second awake.
#define LOWBAT_RETRY_MS (10UL * 60UL * 1000UL)

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
