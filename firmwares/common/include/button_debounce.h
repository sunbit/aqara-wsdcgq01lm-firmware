#ifndef BUTTON_DEBOUNCE_H
#define BUTTON_DEBOUNCE_H
#include <stdint.h>

typedef struct {
    uint8_t pressed;
    uint8_t count;
} button_debounce;

/* Three 10 ms observations to accept an edge. Return +1 press / -1 release. */
int button_debounce_update(button_debounce *state, int pressed);
#endif
