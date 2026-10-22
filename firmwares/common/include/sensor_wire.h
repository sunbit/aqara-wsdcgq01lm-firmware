#ifndef SENSOR_WIRE_H
#define SENSOR_WIRE_H
#include <stdint.h>

typedef struct {
    void (*address)(uint8_t address, int read, void *context);
    void (*write)(uint8_t value, void *context);
    uint8_t (*read)(void *context);
    int (*transfer)(int start, int stop, int read, int nack, void *context);
    int (*wait_us)(uint32_t microseconds, void *context);
    void (*stage)(const char *name, unsigned byte, void *context);
} sht_wire_io;

/* Send a big-endian 16-bit command, STOP, wait, then read a CRC-bearing reply.
 * ACK every reply byte except the final byte, which receives NACK + STOP.
 * A zero-length reply is a write-only command (e.g. sensor soft reset).
 * Callers release/recover the bus after any transport failure. */
int sht_command_read(const sht_wire_io *io, void *context, uint8_t address,
                     uint16_t command, uint32_t wait_us, uint8_t *reply,
                     unsigned length);
#endif
