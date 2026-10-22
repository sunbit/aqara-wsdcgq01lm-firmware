#ifndef SENSOR_POLICY_H
#define SENSOR_POLICY_H
/* Defaults preserve the original30s/120s builds. The production profile is
 * selected explicitly by prepare-build and recorded in its metadata. */
#ifndef SENSOR_SAMPLE_SECONDS
#define SENSOR_SAMPLE_SECONDS 30
#endif
#ifndef SENSOR_TEMPERATURE_MAX_SECONDS
#define SENSOR_TEMPERATURE_MAX_SECONDS 120
#endif
#define SENSOR_TEMPERATURE_DELTA_CENTI 10
#define SENSOR_HUMIDITY_MAX_SECONDS 600
#if SENSOR_SAMPLE_SECONDS < 1 || (3600 % SENSOR_SAMPLE_SECONDS) != 0
#error Sample interval must divide the hourly battery cadence
#endif
#if SENSOR_TEMPERATURE_MAX_SECONDS < SENSOR_SAMPLE_SECONDS || SENSOR_TEMPERATURE_MAX_SECONDS > 3600 || (SENSOR_TEMPERATURE_MAX_SECONDS % SENSOR_SAMPLE_SECONDS) != 0
#error Temperature heartbeat must be a whole number of sampling cycles
#endif
#if (SENSOR_HUMIDITY_MAX_SECONDS % SENSOR_SAMPLE_SECONDS) != 0
#error Sample interval must divide humidity heartbeat
#endif
#endif
