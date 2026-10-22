#ifndef SENSOR_TIMING_H
#define SENSOR_TIMING_H
#include <stdint.h>

typedef uint32_t (*sht_tick_reader)(void *context);
/* Count a periodically restarting timer without changing its configuration.
 * A missed complete period only lengthens the wait. Budget bounds a stuck clock. */
int sht_wait_ticks(sht_tick_reader read, void *context, uint32_t period,
                   uint32_t required, uint32_t budget);
#endif
