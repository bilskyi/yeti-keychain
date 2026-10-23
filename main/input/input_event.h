// Everything the face can react to, whatever produces it.
//
// Sources (buttons, LIS3DH, later BLE) translate their hardware into these
// events; the face and the power policy only ever see the events. Adding a new
// input is therefore a new source file, never a change to the face.
//
// Pure C, no ESP-IDF, so the face and the host tests can include it.

#pragma once

typedef enum {
    INPUT_NONE = 0,

    // LIS3DH (FEATURE_ACCEL)
    INPUT_TAP,
    INPUT_DOUBLE_TAP,
    INPUT_SHAKE,
    INPUT_TILT_LEFT,
    INPUT_TILT_RIGHT,

    // Buttons (FEATURE_BUTTONS)
    INPUT_BTN_A_SHORT,
    INPUT_BTN_A_LONG,
    INPUT_BTN_B_SHORT,
    INPUT_BTN_B_LONG,
    INPUT_BTN_C_SHORT,
    INPUT_BTN_C_LONG,

    INPUT_EVENT_COUNT
} input_event_t;

const char *input_event_name(input_event_t ev);
