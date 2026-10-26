#include "input_event.h"

const char *input_event_name(input_event_t ev) {
    switch (ev) {
        case INPUT_NONE: return "NONE";
        case INPUT_TAP: return "TAP";
        case INPUT_DOUBLE_TAP: return "DOUBLE_TAP";
        case INPUT_SHAKE: return "SHAKE";
        case INPUT_TILT_LEFT: return "TILT_LEFT";
        case INPUT_TILT_RIGHT: return "TILT_RIGHT";
        case INPUT_BTN_A_SHORT: return "BTN_A_SHORT";
        case INPUT_BTN_A_LONG: return "BTN_A_LONG";
        case INPUT_BTN_B_SHORT: return "BTN_B_SHORT";
        case INPUT_BTN_B_LONG: return "BTN_B_LONG";
        case INPUT_BTN_C_SHORT: return "BTN_C_SHORT";
        case INPUT_BTN_C_LONG: return "BTN_C_LONG";
        case INPUT_BTN_A_PRESS: return "BTN_A_PRESS";
        case INPUT_BTN_B_PRESS: return "BTN_B_PRESS";
        case INPUT_BTN_C_PRESS: return "BTN_C_PRESS";
        case INPUT_EVENT_COUNT: break;
    }
    return "?";
}
