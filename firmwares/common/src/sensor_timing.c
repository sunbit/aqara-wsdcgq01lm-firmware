#include "sensor_timing.h"

int sht_wait_ticks(sht_tick_reader read, void *context, uint32_t period,
                   uint32_t required, uint32_t budget)
{
    uint32_t previous, current, elapsed;
    if (!read || !period) return 0;
    if (!required) return 1;
    previous = read(context) % period;
    while (budget--) {
        current = read(context) % period;
        elapsed = current >= previous ? current - previous : period - previous + current;
        if (elapsed >= required) return 1;
        required -= elapsed;
        previous = current;
    }
    return 0;
}
