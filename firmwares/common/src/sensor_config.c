#include <string.h>
#include "sensor_config.h"

void sensor_config_defaults(sensor_config *c)
{
    memset(c, 0, sizeof(*c));
    c->temperature_delta = 10;
    c->humidity_delta = 100;
    c->sample_seconds = 60;
    c->temperature_max_seconds = 300;
    c->humidity_max_seconds = 600;
    c->battery_seconds = 3600;
}

int sensor_config_set(sensor_config *c, uint16_t id, int32_t v)
{
    switch (id) {
    case SENSOR_ATTR_TEMPERATURE_OFFSET:
        if (v < -1000 || v > 1000) return 0;
        c->temperature_offset = (int16_t)v; break;
    case SENSOR_ATTR_HUMIDITY_OFFSET:
        if (v < -2000 || v > 2000) return 0;
        c->humidity_offset = (int16_t)v; break;
    case SENSOR_ATTR_TEMPERATURE_DELTA:
        if (v < 1 || v > 1000) return 0;
        c->temperature_delta = (uint16_t)v; break;
    case SENSOR_ATTR_HUMIDITY_DELTA:
        if (v < 1 || v > 10000) return 0;
        c->humidity_delta = (uint16_t)v; break;
    case SENSOR_ATTR_SAMPLE_SECONDS:
        if (v < 5 || v > 3600) return 0;
        c->sample_seconds = (uint16_t)v; break;
    case SENSOR_ATTR_TEMPERATURE_MIN_SECONDS:
    case SENSOR_ATTR_HUMIDITY_MIN_SECONDS:
        if (v < 0 || v > 3600) return 0;
        if (id == SENSOR_ATTR_TEMPERATURE_MIN_SECONDS) c->temperature_min_seconds = (uint16_t)v;
        else c->humidity_min_seconds = (uint16_t)v;
        break;
    case SENSOR_ATTR_TEMPERATURE_MAX_SECONDS:
    case SENSOR_ATTR_HUMIDITY_MAX_SECONDS:
    case SENSOR_ATTR_BATTERY_SECONDS:
        if (v < 5 || v > 86400) return 0;
        if (id == SENSOR_ATTR_TEMPERATURE_MAX_SECONDS) c->temperature_max_seconds = (uint32_t)v;
        else if (id == SENSOR_ATTR_HUMIDITY_MAX_SECONDS) c->humidity_max_seconds = (uint32_t)v;
        else c->battery_seconds = (uint32_t)v;
        break;
    default: return 0;
    }
    return 1;
}

int sensor_config_valid(const sensor_config *c)
{
    sensor_config copy = *c;
    return sensor_config_set(&copy, SENSOR_ATTR_TEMPERATURE_OFFSET, c->temperature_offset) &&
        sensor_config_set(&copy, SENSOR_ATTR_HUMIDITY_OFFSET, c->humidity_offset) &&
        sensor_config_set(&copy, SENSOR_ATTR_TEMPERATURE_DELTA, c->temperature_delta) &&
        sensor_config_set(&copy, SENSOR_ATTR_HUMIDITY_DELTA, c->humidity_delta) &&
        sensor_config_set(&copy, SENSOR_ATTR_SAMPLE_SECONDS, c->sample_seconds) &&
        sensor_config_set(&copy, SENSOR_ATTR_TEMPERATURE_MIN_SECONDS, c->temperature_min_seconds) &&
        sensor_config_set(&copy, SENSOR_ATTR_HUMIDITY_MIN_SECONDS, c->humidity_min_seconds) &&
        sensor_config_set(&copy, SENSOR_ATTR_TEMPERATURE_MAX_SECONDS, c->temperature_max_seconds) &&
        sensor_config_set(&copy, SENSOR_ATTR_HUMIDITY_MAX_SECONDS, c->humidity_max_seconds) &&
        sensor_config_set(&copy, SENSOR_ATTR_BATTERY_SECONDS, c->battery_seconds);
}

int16_t sensor_calibrate_temperature(int16_t measured, int16_t offset)
{
    int32_t value = (int32_t)measured + offset;
    if (value < -4000) value = -4000;
    if (value > 12500) value = 12500;
    return (int16_t)value;
}

uint16_t sensor_calibrate_humidity(uint16_t measured, int16_t offset)
{
    int32_t value = (int32_t)measured + offset;
    if (value < 0) value = 0;
    if (value > 10000) value = 10000;
    return (uint16_t)value;
}
