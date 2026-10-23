#ifndef SENSOR_BUS_PROFILE_H
#define SENSOR_BUS_PROFILE_H

#ifndef SENSOR_I2C_PROFILE
#define SENSOR_I2C_PROFILE 0
#endif

/* Compare one parameter at a time. 16 MHz / (5 * (prescaler + 1)),
 * per NXP JN-UG-3087. No extra reset, GPIO drive or conversion-mode change. */
#if SENSOR_I2C_PROFILE == 0
#define SENSOR_BUS_PRESCALER 31
#define SENSOR_BUS_SETTLE_US 100UL
#elif SENSOR_I2C_PROFILE == 1
#define SENSOR_BUS_PRESCALER 127
#define SENSOR_BUS_SETTLE_US 100UL
#elif SENSOR_I2C_PROFILE == 2
#define SENSOR_BUS_PRESCALER 31
#define SENSOR_BUS_SETTLE_US 5000UL
#else
#error "Unknown sensor I2C comparison profile"
#endif

#endif
