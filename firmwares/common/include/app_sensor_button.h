#ifndef APP_SENSOR_BUTTON_H
#define APP_SENSOR_BUTTON_H
#include <jendefs.h>

#define SENSOR_BUTTON_MASK (1UL << 5) /* ht01 v7 physical trace: active low. */
PUBLIC void APP_vConfigureButtons(void);
PUBLIC void APP_cbTimerButtonScan(void *context);
PUBLIC void APP_vSensorButtonInterrupt(uint32 status);
PUBLIC void APP_vSensorButtonWake(uint32 status);
PUBLIC bool_t APP_bSensorButtonDebouncing(void);
#endif
