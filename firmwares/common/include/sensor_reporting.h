#ifndef SENSOR_REPORTING_H
#define SENSOR_REPORTING_H
#include <stdint.h>
typedef struct {
    int32_t previous;
    uint32_t age;
    uint8_t have_previous;
} sensor_report_state;
void sensor_report_elapsed(sensor_report_state *state, uint32_t seconds);
int sensor_report_due(const sensor_report_state *state, int32_t value,
                      uint16_t delta, uint16_t minimum, uint32_t maximum, int force);
void sensor_report_sent(sensor_report_state *state, int32_t value);
#endif
