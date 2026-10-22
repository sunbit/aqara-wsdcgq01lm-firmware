#ifndef SENSOR_PROBE_H
#define SENSOR_PROBE_H
#include <stdint.h>

typedef struct {
    unsigned location;
    uint8_t address;
    int known;
} sht_route;

/* A transport attempt must release the bus even when it fails. */
typedef int (*sht_transport)(unsigned location, uint8_t address,
                             uint8_t data[6], void *context);

/* Try a cached route first, then the four supported pin/address combinations.
 * A route is accepted only after both Sensirion CRCs pass. No output mutation
 * on failure, and a failed cached route is not tried twice in the same call. */
int sht_probe(sht_route *route, sht_transport read, void *context,
              int16_t *temperature, uint16_t *humidity);
#endif
