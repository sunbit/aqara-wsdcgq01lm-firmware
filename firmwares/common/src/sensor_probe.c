#include "sensor_probe.h"
#include "sensor_math.h"

static int attempt(sht_route *route, sht_transport read, void *context,
                   unsigned location, uint8_t address,
                   int16_t *temperature, uint16_t *humidity)
{
    uint8_t data[6];
    if (!read(location, address, data, context) ||
        !sht_decode(data, temperature, humidity)) return 0;
    route->location = location;
    route->address = address;
    route->known = 1;
    return 1;
}

int sht_probe(sht_route *route, sht_transport read, void *context,
              int16_t *temperature, uint16_t *humidity)
{
    sht_route previous = *route;
    unsigned location, address;
    route->known = 0;
    if (previous.known && previous.location < 2 &&
        (previous.address == 0x44 || previous.address == 0x45) &&
        attempt(route, read, context, previous.location, previous.address,
                temperature, humidity)) return 1;
    for (location = 0; location < 2; ++location) {
        for (address = 0x44; address <= 0x45; ++address) {
            if (previous.known && previous.location == location &&
                previous.address == address) continue;
            if (attempt(route, read, context, location, (uint8_t)address,
                        temperature, humidity)) return 1;
        }
    }
    return 0;
}
