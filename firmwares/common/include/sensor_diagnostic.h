#ifndef SENSOR_DIAGNOSTIC_H
#define SENSOR_DIAGNOSTIC_H
#include <string.h>

/* Diagnostic profile only. A code describes the confirmed DIO14/15, 0x44
 * route; later probes of absent routes must not hide its original failure. */
enum {
    SENSOR_DIAG_OK = 1,
    SENSOR_DIAG_WRITE_ADDRESS = 2,
    SENSOR_DIAG_READ_ADDRESS = 3,
    SENSOR_DIAG_CRC = 4,
    SENSOR_DIAG_TIMEOUT = 5,
    SENSOR_DIAG_TRANSPORT = 6,
    SENSOR_DIAG_CLUSTER = 7,
    SENSOR_DIAG_REPORT = 8,
    SENSOR_DIAG_NOT_JOINED = 9
};

static inline unsigned sensor_diagnostic_transport(int received, int crc_valid,
                                                  const char *stage, const char *fault)
{
    if (received) return crc_valid ? SENSOR_DIAG_OK : SENSOR_DIAG_CRC;
    if (fault && (strcmp(fault, "tick-timeout") == 0 ||
                  strcmp(fault, "transfer-timeout") == 0)) return SENSOR_DIAG_TIMEOUT;
    if (fault && stage && strcmp(fault, "nack") == 0) {
        if (strcmp(stage, "write-address") == 0) return SENSOR_DIAG_WRITE_ADDRESS;
        if (strcmp(stage, "read-address") == 0) return SENSOR_DIAG_READ_ADDRESS;
    }
    return SENSOR_DIAG_TRANSPORT;
}
#endif
