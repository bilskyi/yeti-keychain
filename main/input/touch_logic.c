#include "input/touch_logic.h"

#include "config.h"

void touch_init(touch_t *t) {
    *t = (touch_t){0};
    button_init(&t->b);
}

input_event_t touch_update(touch_t *t, bool touched, uint32_t now) {
    const button_result_t r = button_update(&t->b, touched, now);
    if (t->held && !t->b.stable) {  // let go after petting
        t->held = false;
        return INPUT_TOUCH_END;
    }
    switch (r) {
        case BUTTON_SHORT:
            if (t->taps == 0 || (int32_t)(now - t->first_tap) > TOUCH_TICKLE_MS) {
                t->taps = 0;
                t->first_tap = now;
            }
            if (++t->taps >= TOUCH_TICKLE_COUNT) {
                t->taps = 0;
                return INPUT_TICKLE;
            }
            return INPUT_TOUCH;
        case BUTTON_LONG:
            t->held = true;
            t->taps = 0;
            return INPUT_TOUCH_HOLD;
        case BUTTON_PRESS:
        case BUTTON_NONE:
            break;
    }
    return INPUT_NONE;
}

bool touch_busy(const touch_t *t) {
    return button_busy(&t->b) || t->held;
}
