#ifndef SENSOR_MATH_H
#define SENSOR_MATH_H
#include <stdint.h>
/* All temperatures are signed hundredths of a degree; humidity is 0.01 %RH. */
uint8_t sht_crc(const uint8_t *data, unsigned length);
int sht_decode(const uint8_t data[6], int16_t *temperature, uint16_t *humidity);
int report_due(int32_t value, int32_t previous, unsigned delta,
               unsigned age_seconds, unsigned maximum_seconds, int have_previous);
#endif
