#include "button_debounce.h"

int button_debounce_update(button_debounce *state, int pressed)
{
    pressed = !!pressed;
    if (pressed == state->pressed) {
        state->count = 0;
    } else if (++state->count == 3) {
        state->pressed = (uint8_t)pressed;
        state->count = 0;
        return pressed ? 1 : -1;
    }
    return 0;
}
