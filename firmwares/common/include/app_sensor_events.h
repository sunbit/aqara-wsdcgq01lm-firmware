#ifndef APP_SENSOR_EVENTS_H
#define APP_SENSOR_EVENTS_H
#include <jendefs.h>
PUBLIC void APP_vSendSensorTimerEvent(void);
PUBLIC void APP_vSendFreshSensorEvent(void);
PUBLIC void APP_vSendConfigurationEvent(void);
#endif
