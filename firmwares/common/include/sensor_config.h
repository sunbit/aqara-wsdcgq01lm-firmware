#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H
#include <stdint.h>

/* Manufacturer-specific attributes in cluster 0xfc00, endpoint 1.
 * Units on the wire are hundredths of degrees/percent and whole seconds. */
enum {
    SENSOR_ATTR_TEMPERATURE_OFFSET = 0x0010,
    SENSOR_ATTR_HUMIDITY_OFFSET,
    SENSOR_ATTR_TEMPERATURE_DELTA,
    SENSOR_ATTR_HUMIDITY_DELTA,
    SENSOR_ATTR_SAMPLE_SECONDS,
    SENSOR_ATTR_TEMPERATURE_MAX_SECONDS,
    SENSOR_ATTR_HUMIDITY_MAX_SECONDS,
    SENSOR_ATTR_BATTERY_SECONDS,
    SENSOR_ATTR_TEMPERATURE_MIN_SECONDS,
    SENSOR_ATTR_HUMIDITY_MIN_SECONDS
};

typedef struct {
    int16_t temperature_offset, humidity_offset;
    uint16_t temperature_delta, humidity_delta, sample_seconds;
    uint16_t temperature_min_seconds, humidity_min_seconds;
    uint32_t temperature_max_seconds, humidity_max_seconds, battery_seconds;
} sensor_config;

void sensor_config_defaults(sensor_config *config);
/* Return zero for unknown attributes or out-of-range values; no partial edits. */
int sensor_config_set(sensor_config *config, uint16_t attribute, int32_t value);
int sensor_config_valid(const sensor_config *config);
int16_t sensor_calibrate_temperature(int16_t measured, int16_t offset);
uint16_t sensor_calibrate_humidity(uint16_t measured, int16_t offset);
#endif
