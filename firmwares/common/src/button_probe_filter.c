#include "button_probe_filter.h"
#include <string.h>

void button_probe_init(button_probe_filter *state, uint32_t mask, uint32_t input)
{
    memset(state, 0, sizeof(*state));
    state->mask = mask & ((1UL << BUTTON_PROBE_DIOS) - 1);
    state->baseline = state->stable = input & state->mask;
}

uint32_t button_probe_update(button_probe_filter *state, uint32_t input)
{
    uint32_t changed = 0;
    unsigned dio;
    for (dio = 0; dio < BUTTON_PROBE_DIOS; ++dio) {
        uint32_t bit = 1UL << dio;
        if (!(state->mask & bit)) continue;
        if (!((input ^ state->stable) & bit)) {
            state->pending[dio] = 0;
        } else if (++state->pending[dio] == BUTTON_PROBE_DEBOUNCE) {
            state->stable ^= bit;
            state->pending[dio] = 0;
            if (state->edges[dio] != UINT16_MAX) ++state->edges[dio];
            changed |= bit;
        }
    }
    return changed;
}

uint32_t button_probe_candidates(const button_probe_filter *state)
{
    uint32_t candidates = 0;
    unsigned dio;
    for (dio = 0; dio < BUTTON_PROBE_DIOS; ++dio) {
        uint32_t bit = 1UL << dio;
        if ((state->mask & bit) && state->edges[dio] >= 4 &&
            !(state->edges[dio] & 1) && !((state->stable ^ state->baseline) & bit))
            candidates |= bit;
    }
    return candidates;
}
