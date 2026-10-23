#ifndef DIAGNOSTIC_SUITE_H
#define DIAGNOSTIC_SUITE_H
#include <stdint.h>

#define DIAG_CASES 16
#define DIAG_ROW_BYTES 16
#define DIAG_RECORD_BYTES (16 + DIAG_CASES * DIAG_ROW_BYTES)
#define DIAG_PACKET_BYTES 24
#define DIAG_FLAG_LED 1
#define DIAG_FLAG_NO_PULLUP 2
#define DIAG_FLAG_CLEAR_BUS 4
#define DIAG_FLAG_SOFT_RESET 8
#define DIAG_FLAG_GPIO 16
#define DIAG_FLAG_LONG_SETTLE 32
#define DIAG_FLAG_NO_FILTER 64
#ifdef SENSOR_GPIO_I2C_DIAGNOSTICS
#define DIAG_PROTOCOL 2
#else
#define DIAG_PROTOCOL 1
#endif
typedef struct {
    uint8_t location, address, prescaler, settle_units, flags;
    uint16_t command;
} diag_case;
typedef struct { uint8_t bytes[DIAG_RECORD_BYTES]; } diag_record;

const diag_case *diag_case_at(unsigned index);
void diag_record_begin(diag_record *record, uint32_t run);
uint8_t *diag_record_row(diag_record *record, unsigned index);
void diag_record_finish(diag_record *record);
int diag_record_valid(const diag_record *record);
uint32_t diag_record_run(const diag_record *record);
/* index 0..15 is a case; 16 is a summary packet. No native struct on wire. */
void diag_packet(const diag_record *record, unsigned index, unsigned durable,
                 unsigned replay, uint8_t packet[DIAG_PACKET_BYTES]);
#endif
