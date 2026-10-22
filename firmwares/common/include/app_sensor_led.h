#ifndef APP_SENSOR_LED_H
#define APP_SENSOR_LED_H
#include <jendefs.h>

/* ht01: owner observed all three DIO9 LOW pulses illuminate the LED. */
#define SENSOR_LED_MASK (1UL << 9)
#define SENSOR_LED_BLINK_MS 150
PUBLIC void APP_vSetupLeds(void);
PUBLIC void APP_vSensorLedPress(void);
PUBLIC void APP_vSensorLedPairing(void);
PUBLIC void APP_cbTimerLedBlink(void *context);
#endif
