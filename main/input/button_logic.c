#include "input/button_logic.h"

#include "config.h"

void button_init(button_t *b) {
    *b = (button_t){0};
}

button_result_t button_update(button_t *b, bool pressed, uint32_t now) {
    if (pressed != b->raw) {
        b->raw = pressed;
        b->raw_since = now;
    }
    if (b->raw != b->stable && (int32_t)(now - b->raw_since) >= BTN_DEBOUNCE_MS) {
        b->stable = b->raw;
        if (b->stable) {
            b->pressed_at = b->raw_since;
            b->long_sent = false;
            return BUTTON_PRESS;
        }
        if (!b->long_sent) {
            return BUTTON_SHORT;
        }
    }
    if (b->stable && !b->long_sent && (int32_t)(now - b->pressed_at) >= BTN_LONG_MS) {
        b->long_sent = true;
        return BUTTON_LONG;
    }
    return BUTTON_NONE;
}

bool button_busy(const button_t *b) {
    return b->raw || b->stable;
}
