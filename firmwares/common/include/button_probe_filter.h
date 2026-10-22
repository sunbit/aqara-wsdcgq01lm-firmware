#ifndef BUTTON_PROBE_FILTER_H
#define BUTTON_PROBE_FILTER_H
#include <stdint.h>

#define BUTTON_PROBE_DIOS 20
/* Four consecutive 5 ms observations before accepting each individual edge. */
#define BUTTON_PROBE_DEBOUNCE 4
typedef struct {
    uint32_t mask, baseline, stable;
    uint8_t pending[BUTTON_PROBE_DIOS];
    uint16_t edges[BUTTON_PROBE_DIOS];
} button_probe_filter;

void button_probe_init(button_probe_filter *state, uint32_t mask, uint32_t input);
uint32_t button_probe_update(button_probe_filter *state, uint32_t input);
/* Only a diagnostic candidate: still correlate the trace with physical presses. */
uint32_t button_probe_candidates(const button_probe_filter *state);
#endif
