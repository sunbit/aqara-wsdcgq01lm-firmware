#include "sensor_wire.h"

int sht_command_read(const sht_wire_io *io, void *context, uint8_t address,
                     uint16_t command, uint32_t wait_us, uint8_t *reply,
                     unsigned length)
{
    unsigned i;
    if (!io || !io->address || !io->write || !io->transfer || !io->wait_us ||
        !io->stage || (length && (!reply || !io->read)) || length > 6 ||
        address > 0x7f) return 0;
    io->stage("write-address", 0, context);
    io->address(address, 0, context);
    if (!io->transfer(1, 0, 0, 0, context)) return 0;
    io->stage("command-msb", 0, context);
    io->write((uint8_t)(command >> 8), context);
    if (!io->transfer(0, 0, 0, 0, context)) return 0;
    io->stage("command-lsb", 1, context);
    io->write((uint8_t)command, context);
    if (!io->transfer(0, 1, 0, 0, context)) return 0;
    if (wait_us) {
        io->stage("command-wait", 0, context);
        if (!io->wait_us(wait_us, context)) return 0;
    }
    if (length) {
        io->stage("read-address", 0, context);
        io->address(address, 1, context);
        if (!io->transfer(1, 0, 0, 0, context)) return 0;
        for (i = 0; i < length; ++i) {
            int last = i == length - 1;
            io->stage("read-data", i, context);
            if (!io->transfer(0, last, 1, last, context)) return 0;
            reply[i] = io->read(context);
        }
    }
    io->stage("complete", length ? length - 1 : 0, context);
    return 1;
}
